/*
 * depth_sensor_plugin.cpp
 *
 *  Created on: Apr 5, 2017
 *      Author: kevin
 */

#include "depth_sensor_plugin.h"

#include <fstream>
#include <sstream>

//#include <QtGui/QImage>
//#include <QtGui/QPixmap>

#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/io/ply_io.h>
#include <pcl/filters/fast_bilateral_omp.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <gazebo/msgs/request.pb.h>
#include <gazebo/gazebo.hh>
#include <gazebo/sensors/sensors.hh>

#include "/home/kevin/research/gazebo/msgs/include/point_cloud.pb.h"

#include "perlin_noise.h"
#include "cvmat_serialization.h"

#define COUT_PREFIX "\033[1;32m" << "[DepthSensorPlugin] " << "\033[0m"
#define CERR_PREFIX "\033[1;31m" << "[DepthSensorPlugin]" << "\033[0m"


using namespace std;
using namespace gazebo;

DepthSensorPlugin::DepthSensorPlugin()
	: m_scene_mgr( NULL ),
	  m_ogre_camera( NULL ),
	  SENSOR_IR_PROJECTOR_NAME_PREFIX( "Sensor_IR_Projector_" ),
	  m_rgb_rt_listener( NULL ),
	  m_depth_rt_listener( NULL ),
	  m_rayconf_rt_listener( NULL ),
	  m_take_picture( false ),
	  m_segment_rt_listener( NULL ),
	  m_use_ideal_segmentation( true )
	// TODO initialize class variable
{
}

DepthSensorPlugin::~DepthSensorPlugin()
{
	// don't know if this is necessary
	this->m_camera_sensor->DisconnectUpdated( this->m_update_connection );

	// free container
	m_turned_off_lights.clear();
	m_turned_off_mobj.clear();
	m_cloned_entity.clear();

	// free render texture
	m_rgb_rt->removeAllListeners();
	m_depth_rt->removeAllListeners();
	m_rayconf_rt->removeAllListeners();

	m_segment_rt->removeAllListeners();

	// free frame buffer
	delete []m_rgb_buffer;
	delete []m_depth_buffer;
	delete []m_rayconf_buffer;
	delete []m_segment_buffer;
}

void DepthSensorPlugin::Load( sensors::SensorPtr _sensor, sdf::ElementPtr _sdf )
{
	// *************** //
	// essential setup //
	// *************** //

	// get camera sensor ptr
	this->m_camera_sensor = std::dynamic_pointer_cast<sensors::CameraSensor>( _sensor );

	// get rendering::CameraPtr
	this->m_camera = m_camera_sensor->GetCamera();

	// get the scene that this camera is in
	this->m_scene = m_camera_sensor->GetCamera()->GetScene();

	// make sure the sensor is valid
	if( !this->m_camera_sensor )
	{
		gzerr << "DepthSensorPlugin requires a CameraSensor.\n";
		return;
	}
	//std::cout << "The sensor is valid!" << std::endl;

	// !! sensor plugin must connect to sensor updated signals, or it will CRASH !! //

	// Connect to the sensor update event.
	this->m_update_connection = this->m_camera_sensor->ConnectUpdated(std::bind(&DepthSensorPlugin::_onUpdatedCallback, this));
	//std::cout << "Connected to the sensor update event." << std::endl;

	// connect to render event ( this signal won't stop when the world is paused, this will occur problem if the world is modified from gzclient, because the world in gzserver didn't updated during paused.
	//this->m_render_connection = event::Events::ConnectPostRender( boost::bind( &DepthSensorPlugin::_preRenderCallback, this ) );

	// Make sure the parent sensor is active.
	this->m_camera_sensor->SetActive( true );
	//std::cout << "The parent sensor is active." << std::endl;

	// get SceneManager & Camera pointer
	m_ogre_camera = m_camera_sensor->GetCamera()->GetOgreCamera();
	m_scene_mgr = m_ogre_camera->getSceneManager();
	//std::cout << "Get SceneManager and Camera pointer" << std::endl;

	// ****************** //
	// setup depth sensor //
	// ****************** //
	std::cout << "\n" << std::endl;
	std::cout << "Setting up depth sensor..." << std::endl;

	this->_loadPlugins();
	//std::cout << "\tFinish _loadPlugins()" << std::endl;
	this->_addResources();
	//std::cout << "\tFinish _addResources()" << std::endl;
	this->_setupDepthSensor();
	//std::cout << "\tFinish _setupDepthSensor()" << std::endl;
	this->_prepareSensorNoise();
	//std::cout << "\tFinish _prepareSensorNoise()" << std::endl;

	// ************************ //
	// setup ideal segmentation //
	// ************************ //
	this->_setupIdealSegmentation();
	//std::cout << "\tFinish _setupIdealSegmentation()" << std::endl;
	std::cout << "Depth sensor completed!" << std::endl;
	std::cout << "\n" << std::endl;

	// ********************* //
	// setup gazebo messages //
	// ********************* //
	m_node_ptr = transport::NodePtr(new transport::Node());
	// Initialize the node with the world name
	m_node_ptr->Init( m_camera_sensor->GetWorldName() );

	if( m_use_ideal_segmentation )
	{
		m_publisher_ptr = m_node_ptr->Advertise< pcl::msgs::PointCloudXYZL >("~/depth_sensor/point_cloud");
	}
	else
	{
		m_publisher_ptr = m_node_ptr->Advertise< pcl::msgs::PointCloud >("~/depth_sensor/point_cloud");
	}
	m_rethrow_publisher_ptr = m_node_ptr->Advertise< gazebo::msgs::Request >("~/depth_sensor/rethrow_event");

	// listen to take picture request from evaluation platform
	m_subscriber_ptr = m_node_ptr->Subscribe( "~/evaluation_platform/take_picture_request", &DepthSensorPlugin::_takePicture, this );
	m_snapshot_subscriber_ptr = m_node_ptr->Subscribe( "~/evaluation_platform/only_snapshot", &DepthSensorPlugin::_onlySnapshot, this );
}

// Calls whenever DepthSensorPlugin is updated //
void DepthSensorPlugin::_onUpdatedCallback()
{
	// action only receiving request
	if( m_take_picture )
	{
		cout << "Sensor start time : " << common::Time::GetWallTimeAsISOString() << endl;

		// TODO : TEMP START
		double time = common::Time::GetWallTime().Double();
		// TEMP END

		// ********************************* //
		// update our render target manually //
		// ********************************* //

		// hide the grid first if it is visible
		rendering::ScenePtr scene = m_camera_sensor->GetCamera()->GetScene();
		unsigned int grid_count = scene->GetGridCount();
		std::vector<uint32_t> visible_grid_id;
		for( unsigned int i = 0; i < grid_count; i++ )
		{
			if( scene->GetGrid( i )->GetSceneNode()->getAttachedObject( 0 )->getVisible() )	// should have only one attachment on this scenenode
			{
				visible_grid_id.push_back( i );
				scene->GetGrid( i )->Enable( false );
			}
		}

		// update the render target
		m_rgb_rt->update( true );
		m_depth_rt->update( true );
		m_rayconf_rt->update( true );

		if( m_use_ideal_segmentation )
		{
			m_segment_rt->update( true );
		}

		// unhide the Grid if it's visible before
		for( unsigned int i = 0; i < visible_grid_id.size(); i++ )
		{
			scene->GetGrid( visible_grid_id[i] )->Enable( true );
		}

		// destroy vector
		visible_grid_id.clear();

		// **************** //
		// save sensor data //
		// **************** //
		this->_saveSensorData();

		// TODO : TEMP START
		cout << "Sensor simulation time : " << common::Time::GetWallTime().Double() - time << endl;
		// TEMP END

		cout << "Sensor finish time : " << common::Time::GetWallTimeAsISOString() << endl;

		// reset m_take_picture
		m_take_picture = false;
	}
}
void DepthSensorPlugin::_onlySnapshot( ConstMsgsRequestPtr &_msgs )
{
	cout << COUT_PREFIX << "ONLY SNAPSHOT MODE" << endl;
	m_snapshot = true;
	std::stringstream ss;
	ss << _msgs->data();
	for(int i=0; i<19; i++)
	{
		ss >> m_msgs[i];
		//std::cout << "m_msgs " << i << " : " << m_msgs[i] << std::endl;
	}
	// check if the msgs exceed the boundary of the image
	for(int i=0; i<9; i++)
	{
		if((m_msgs[2*i+1]>1280)||(m_msgs[2*i+1]<0)||(m_msgs[2*i+2]>960)||(m_msgs[2*i+2]<0))
		{
			m_msgs[2*i+1] = 0;
			m_msgs[2*i+2] = 0;
			std::cout << "the object is out of image boundary...setting the coordinate to (0,0)" << std::endl;
		}
	}
	m_total_snapshot = m_msgs[0];
}

void DepthSensorPlugin::_takePicture( ConstMsgsRequestPtr &_msgs )
{
	cout << COUT_PREFIX << "take picture" << endl;
	m_take_picture = true;
}

void DepthSensorPlugin::_loadPlugins()
{
	std::string required_plugin = "Cg Program Manager";

	// Check if the required plugins are installed and ready for use
	// If not: load the plugin
	std::cout << "\tloading : " << required_plugin << std::endl;
	//Ogre::Root::PluginInstanceList ip = Ogre::Root::getSingletonPtr()->getInstalledPlugins();

	// try to find the required plugin in the current installed plugins
	bool found = false;

	//for( Ogre::Root::PluginInstanceList::iterator k = ip.begin(); k != ip.end(); k++ )
	//{
	//	std::cout << ' ' << (*k) ;
	//	std::cout << "\n" << std::endl;
	//	if( (*k)->getName() == required_plugin )
	//	{
	//		found = true;
	//		std::cout << "\t\tFOUND" << std::endl;
			//std::cout << required_plugin << std::endl;

	//		break;
	//	}
	//}
	// load the required_plugin

	if( !found )
	{
		// load Plugin_CgProgramManager ( this loading plugin method is from gazebo/RenderEngine.cc )
		std::list<std::string> ogrePaths = common::SystemPaths::Instance()->GetOgrePaths();
		/*
		//std::list<std::string> ogrePaths = "/usr/local/lib/OGRE";
		for( std::list<std::string>::iterator iter = ogrePaths.begin(); iter != ogrePaths.end(); iter++ )
		{
			//*iter = '/usr/local/lib/OGRE';
			std::cout << ' ' << *iter ;

			std::string path(*iter);
			DIR *dir = opendir(path.c_str());

			//std::cout << path.c_str() << std::endl;

			if( dir == NULL )
			{
				std::cout << "\t\tNOT FOUND" << std::endl;
				continue;
			}
			closedir( dir );
			std::cout << "\t\t" << path << std::endl;

			std::string prefix = "";
			std::string extension = ".so";

			Ogre::Root::getSingletonPtr()->loadPlugin( path + "/" + prefix + "Plugin_CgProgramManager" + extension );
			std::cout << "\t\tFOUND: " << required_plugin << std::endl;
		}
		*/
		Ogre::Root::getSingletonPtr()->loadPlugin("/usr/local/ogre-1-9/lib/OGRE/Plugin_CgProgramManager.so");

	}
}

void DepthSensorPlugin::_addResources()
{
	// std::cout << "\tIn _addResources()" << std::endl;

	// check if the resourceGroup already exist
	Ogre::ResourceGroupManager *resource_group_mgr = Ogre::ResourceGroupManager::getSingletonPtr();

	if( resource_group_mgr->resourceGroupExists( "DEPTH_SHADOWMAP_RESOURCE_GROUP" ) )
	{
		return;
	}
	// std::cout << "\t\tresourceGroup does not exist! Setting up..." << std::endl;
	std::cout << "\tadding resources" << std::endl;
	// setup resource for shadow map if it's not already exist
	resource_group_mgr->createResourceGroup( "DEPTH_SHADOWMAP_RESOURCE_GROUP" );
	// std::cout << "\t\tcreated: DEPTH_SHADOWMAP_RESOURCE_GROUP" << std::endl;

	resource_group_mgr->addResourceLocation( "/home/kevin/research/gazebo/resource/depth_shadowmap_resource",
											 "FileSystem",
											 "DEPTH_SHADOWMAP_RESOURCE_GROUP",
											 false );
	// std::cout << "\t\tlocation added" << std::endl;

	resource_group_mgr->initialiseResourceGroup( "DEPTH_SHADOWMAP_RESOURCE_GROUP" );
	// std::cout << "\t\tinitialized" << std::endl;
}

void DepthSensorPlugin::_setupDepthSensor()
{
	// ************************* //
	// get necessary information //
	// ************************* //
	// get camera info
	unsigned int cam_w = m_camera_sensor->GetImageWidth();
	unsigned int cam_h = m_camera_sensor->GetImageHeight();
	std::string camera_name = m_camera->GetName();

	// TEMP START //
	cout << "\tcamera name : " << camera_name << endl;
	// TEMP END //

	// ******************** //
	// setup render texture //
	// ******************** //


	// ******************************** //
	// setup DepthSensor's IR Projector //
	// ******************************** //

	// *** START create sensor virtual IR projector ( spotlight )
	Ogre::Light* spotlight = m_scene_mgr->createLight( SENSOR_IR_PROJECTOR_NAME_PREFIX + camera_name );
	spotlight->setType( Ogre::Light::LT_SPOTLIGHT );
	spotlight->setDiffuseColour( 0.0, 0.0, 0.0 );
	spotlight->setSpecularColour( 1.0, 1.0, 1.0 );

	// TEMP START
	//	spotlight->setPosition( 0, 0, 30 );
	//	spotlight->setDirection( 0, 0, -1 );
	// TEMP END

	float inner_angle = 90;
	spotlight->setSpotlightRange( Ogre::Degree( inner_angle ), Ogre::Degree( inner_angle ) );
	spotlight->setVisible( false );	// set visible when sensor is running
	// don't need to set ShadowCamera globally, just set ShadowCamera for our spot light
	spotlight->setCustomShadowCameraSetup( Ogre::ShadowCameraSetupPtr( new Ogre::DefaultShadowCameraSetup() ) );

	Ogre::SceneNode *sensor_cam_projector_node;
	sensor_cam_projector_node = m_ogre_camera->getParentSceneNode()->createChildSceneNode( "Sensor_IR_Projector_Node_" + camera_name );
	sensor_cam_projector_node->attachObject( spotlight );

	// initial distance between camera and IR projector
	m_base_dist = 0.10;
	sensor_cam_projector_node->translate( 0, -m_base_dist, 0, Ogre::Node::TS_PARENT );
	// A light attached to a SceneNode is assumed to have a base position of (0,0,0) and a direction of (0,0,1)
	sensor_cam_projector_node->yaw( Ogre::Degree( 90.0f ) );


	// ****** render to texture START (Depth) *** //
	Ogre::TexturePtr rtt_texture;
	rtt_texture = Ogre::TextureManager::getSingleton().createManual(	"RttTex_DEPTH_" + camera_name,
																		Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
																		Ogre::TEX_TYPE_2D,
																		cam_w,
																		cam_h,
																		0,
																		Ogre::PF_FLOAT32_RGBA,
																		Ogre::TU_RENDERTARGET);

	m_depth_rt = rtt_texture->getBuffer()->getRenderTarget();
	m_depth_rt -> addViewport( m_ogre_camera );
	m_depth_rt -> getViewport(0)->setClearEveryFrame( true );
	m_depth_rt -> getViewport(0)->setBackgroundColour( Ogre::ColourValue::White );
	m_depth_rt -> getViewport(0)->setOverlaysEnabled( false );

	// allocate memory for rgb frame buffer
	m_depth_buffer = new float[ cam_w * cam_h * 4 ];

	// create rgb render target listener
	m_depth_rt_listener = new DepthRTListener( 	m_scene,
												m_camera_sensor->GetCamera(),
												m_scene_mgr,
												m_depth_rt,
												m_depth_buffer,
												&m_shadow_settings,
												&m_turned_off_lights,
												&m_turned_off_mobj,
												&m_cloned_entity,
												&m_base_dist,
												SENSOR_IR_PROJECTOR_NAME_PREFIX );

	m_depth_rt -> addListener( m_depth_rt_listener );

	// ****** render to texture END*** //

	// ****** render to texture START (RayConf) *** //
	rtt_texture = Ogre::TextureManager::getSingleton().createManual(	"RttTex_RayConf_" + camera_name,
																		Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
																		Ogre::TEX_TYPE_2D,
																		cam_w,
																		cam_h,
																		0,
																		Ogre::PF_FLOAT32_RGBA,
																		Ogre::TU_RENDERTARGET);
	m_rayconf_rt = rtt_texture->getBuffer()->getRenderTarget();
	m_rayconf_rt -> addViewport( m_ogre_camera );
	m_rayconf_rt -> getViewport(0)->setClearEveryFrame( true );
	m_rayconf_rt -> getViewport(0)->setBackgroundColour( Ogre::ColourValue::White );
	m_rayconf_rt -> getViewport(0)->setOverlaysEnabled( false );

	// allocate memory for rgb frame buffer
	m_rayconf_buffer = new float[ cam_w * cam_h * 4 ];

	// create rgb render target listener
	m_rayconf_rt_listener = new RayConfRTListener( 	m_scene,
													m_camera_sensor->GetCamera(),
													m_scene_mgr,
													m_rayconf_rt,
													m_rayconf_buffer,
													&m_shadow_settings,
													&m_turned_off_lights,
													&m_turned_off_mobj,
													&m_cloned_entity,
													&m_base_dist,
													SENSOR_IR_PROJECTOR_NAME_PREFIX );

	m_rayconf_rt -> addListener( m_rayconf_rt_listener );

	// ****** render to texture END*** //

	// ************************************** //
	// create render texture for specular map //
	// ************************************** //
	rtt_texture = Ogre::TextureManager::getSingleton().createManual(	"RttTex_RGB_" + camera_name,
																		Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
																		Ogre::TEX_TYPE_2D,
																		cam_w,
																		cam_h,
																		0,
																		Ogre::PF_BYTE_RGB,
																		Ogre::TU_RENDERTARGET );
	m_rgb_rt = rtt_texture->getBuffer()->getRenderTarget();

	// setup render texture
	m_rgb_rt -> addViewport( m_ogre_camera );
	m_rgb_rt -> getViewport(0)->setClearEveryFrame( true );
	// set background as black for specular map
	m_rgb_rt -> getViewport(0)->setBackgroundColour( Ogre::ColourValue( 0, 0, 0, 1 ) );
	m_rgb_rt -> getViewport(0)->setOverlaysEnabled( false );

	// allocate memory for rgb frame buffer
	m_rgb_buffer = new unsigned char[ cam_w * cam_h * 3 ];

	// create rgb render target listener
	m_rgb_rt_listener = new RGBRTListener(	m_scene,
											m_camera_sensor->GetCamera(),
											m_scene_mgr,
											m_rgb_rt,
											m_rgb_buffer,
											&m_base_dist,
											spotlight );

	m_rgb_rt -> addListener( m_rgb_rt_listener );

}

void DepthSensorPlugin::_prepareSensorNoise()
{
	using namespace cv;

	// ************* //
	// prepare noise //
	// ************* //

	// read noise magnitude from file
	cv::Mat noise_mag;

	std::cout << "\treading mag.bin to set up noise" << std::endl;
	std::ifstream ifs( "mag.bin", std::ios::in | std::ios::binary );

	{
		boost::archive::binary_iarchive ia( ifs );

		ia >> noise_mag;
	}

	ifs.close();

	// resize noise to current sensor resolution
	float scale = (float)noise_mag.cols * noise_mag.rows / ( m_camera->GetImageWidth() * m_camera->GetImageHeight() );
	cout << "\t\tscale: " << scale << endl;
	cout << "\t\tsqrt scale: " << sqrt( scale ) << endl;
	cv::resize( noise_mag, noise_mag, cv::Size( m_camera->GetImageWidth(), m_camera->GetImageHeight() ) );

	// generate random phase
	cv::Mat noise_phase( noise_mag.rows, noise_mag.cols, noise_mag.type() );

	for( int j = 0; j < noise_phase.rows; j++ )
	{
		for( int i = 0; i< noise_phase.cols; i++ )
		{
			if( noise_mag.at<float>( j, i ) != 0 )
			{
				int rand_num = rand() % 36000;
				float degree = rand_num / 100.0 - 180.0;

				noise_phase.at<float>( j, i ) = degree * M_PI / 180;
			}
			else
			{
				noise_phase.at<float>( j, i ) = 0;
			}
		}
	}

	Mat complex;
	// change magnitude ^ phase => Real part & Imaginary part
	Mat real_part, img_part;
	polarToCart( noise_mag, noise_phase, real_part, img_part );
	Mat complex_temp_r[]={ real_part, img_part };
	merge( complex_temp_r, 2, complex );

	// generate sensor noise from frequency domain
	dft( complex, m_noise, DFT_INVERSE | DFT_REAL_OUTPUT | DFT_SCALE  );

	// scale the noise dude to resizing noise_mag
	m_noise.convertTo( m_noise, m_noise.type(), 1 / sqrt(scale) );
}

void DepthSensorPlugin::_check_file_number()
{
	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir ("/home/kevin/research/gazebo/evaluation_platform/only_snapshot/rgb")) != NULL) {
		/* print all the files and directories within directory */
		while ((ent = readdir (dir)) != NULL)
		{
			m_current_file_number++;
			//printf ("%s\n", ent->d_name);
		}
		m_current_file_number = m_current_file_number - 2;
		closedir (dir);
		std::cout << "it has : " << m_current_file_number << " files in directory rgb and pcd" << std::endl;
	}
	else
	{
		/* could not open directory */
		return;
	}
}

void DepthSensorPlugin::_saveSensorData()
{
	if((m_snapshot)&&(m_save_count>m_total_snapshot))
	{
		return;
	}
	// **************** //
	// save sensor data //
	// **************** //

 	// get sensor info
	int width = m_rgb_rt->getWidth();
	int height = m_rgb_rt->getHeight();

	// ******** //
	// save RGB //
	// ******** //

	// save rgb picture from gazebo's sensor
	if(m_snapshot)
	{
		// check the total files in the directory (ONLY ONCE!!!)
		if(m_current_file_number == 0) { _check_file_number(); }
		boost::filesystem::create_directory( "only_snapshot/rgb" );
		std::cout << COUT_PREFIX << "Snapshot progress = " << m_save_count << " / " << m_total_snapshot << std::endl;
		ss.clear();
		ss.str( "" );
		// get the path for saving
		save_string.clear();
		m_save_file_number = m_save_count + m_current_file_number;
		ss << "only_snapshot/rgb/rgb_" << m_save_file_number << ".png";
		save_string = ss.str();
		this->m_camera_sensor->SaveFrame( save_string );
		std::cout << "save rgb = " << save_string << endl;
	}
	else
	{
		this->m_camera_sensor->SaveFrame( "rgb_sensor.png" );
	}

	// TODO: TEMP START test rgb render texture
	// QImage rgb_image( m_rgb_buffer, width, height, QImage::Format_RGB888 );
	// rgb_image.save( "saturate_map.png" );
	// TEMP END



	// ****************** //
	// extract depth data //
	// ****************** //

	// TODO : TEMP START
	// double time = common::Time::GetWallTime().Double();
	// TEMP END

	unsigned char *temp_depth_buffer = new unsigned char[ width * height * 3 ];
	// convert float 32 to unsigned char 8 bit;
	for( unsigned int i = 0; i < m_depth_rt->getWidth() * m_depth_rt->getHeight(); i++ )
	{
		unsigned char data = (unsigned char)( m_depth_buffer[ 4 * i + 0] * 255 );
		temp_depth_buffer[3*i] = data;
		temp_depth_buffer[3*i + 1] = data;
		temp_depth_buffer[3*i + 2] = data;
	}
	// QImage depth_image( temp_depth_buffer, width, height, QImage::Format_RGB888 );
	// depth_image.save( "depth.png" );

	// TODO : TEMP START
	// cout << "extract depth data : " << common::Time::GetWallTime().Double() - time << endl;
	// time = common::Time::GetWallTime().Double();
	// TEMP END

	// ************************** //
	// intensity saturation check //
	// ************************** //
	vector< float > noise_5_temp = perlinNoise( width, height, 5 );	// amplitude about -2.5~2.5
	vector< float > noise_10_temp = perlinNoise( width, height, 10 );	// amplitude about -5 ~ 5
	vector< float > noise_20_temp = perlinNoise( width, height, 20 );	// amplitude about -10 ~ 10
	vector< float > noise_40_temp = perlinNoise( width, height, 40 );	// amplitude about -10 ~ 10

	for( int j = 0; j < height; j++ )
	{
		for( int i = 0; i < width; i++ )
		{
			int idx = i + j * width;

			//float thres = 180 + noise_5_temp[ idx ]*5 + noise_10_temp[ idx ]*4 + noise_20_temp[ idx ]*10 + noise_40_temp[ idx ] * 10;
			float thres = 180 + noise_40_temp[ idx ] * 10;
			thres = thres > 0 ? thres : 1;

			if( m_rgb_buffer[ 3 * idx ] > thres  )
			{
				temp_depth_buffer[ 3 * idx ] = 255;
				temp_depth_buffer[ 3 * idx + 1 ] = 255;
				temp_depth_buffer[ 3 * idx + 2 ] = 255;
			}

		}
	}
	//	depth_image.save( "depth_intensity_saturate.png" );

	// ********************** //
	// disturb occlusion edge //
	// ********************** //
	_disturbOcclusionEdge( temp_depth_buffer );
	//_disturbOcclusionEdge( temp_depth_buffer );
	//	depth_image.save( "depth_disturb_occlusion.png" );

	// TODO : TEMP START
	//	cout << "disturb edge : " << common::Time::GetWallTime().Double() - time << endl;
	//	time = common::Time::GetWallTime().Double();
	// TEMP END

	// ************************** //
	// confidence threshold check //
	// ************************** //
	vector< float > noise_5 = perlinNoise( width, height, 5 );	// amplitude about -2.5~2.5
	vector< float > noise_10 = perlinNoise( width, height, 10 );	// amplitude about -5 ~ 5
	vector< float > noise_20 = perlinNoise( width, height, 20 );	// amplitude about -10 ~ 10

	for( int j = 0; j < height; j++ )
	{
		for( int i = 0; i < width; i++ )
		{
			int idx = i + j * width;
			// disturb confidence threshold with pelin noise
			float conf_thres = 75.f + noise_5[ idx ] + noise_10[ idx ] + noise_20[ idx ];

			if( m_rayconf_buffer[ 4 * idx + 3 ] < cos( conf_thres * M_PI / 180.f ) )
			{
				temp_depth_buffer[ 3 * idx ] = 255;
				temp_depth_buffer[ 3 * idx + 1 ] = 255;
				temp_depth_buffer[ 3 * idx + 2 ] = 255;
			}

		}
	}

	//	depth_image.save( "depth_disturb_occlusion_conf.png" );

	// TODO : TEMP START
	//	cout << "confidence check : " << common::Time::GetWallTime().Double() - time << endl;
	//	time = common::Time::GetWallTime().Double();
	// TEMP END

	// **************** //
	// save point cloud //
	// **************** //
	pcl::PointCloud<pcl::PointXYZ> cloud;
	cloud.width = width;
	cloud.height = height;
	cloud.is_dense = false;
	cloud.points.resize( cloud.width * cloud.height );

	// ideal data
	/*for( unsigned int idx = 0 ; idx < cloud.size(); ++idx )
	{

		// check validatioin of data, in depth range & confidence != 0
		if( m_rayconf_buffer[ 4 * idx + 3 ] != -1  && m_rayconf_buffer[ 4 * idx + 2 ] < 0 )
		{
			cloud[idx].x = m_rayconf_buffer[ 4 * idx + 0 ];
			cloud[idx].y = m_rayconf_buffer[ 4 * idx + 1 ];
			cloud[idx].z = m_rayconf_buffer[ 4 * idx + 2 ];
		}
		else
		{
			cloud[idx].x = std::numeric_limits<float>::quiet_NaN();
			cloud[idx].y = std::numeric_limits<float>::quiet_NaN();
			cloud[idx].z = std::numeric_limits<float>::quiet_NaN();

		}

	}

	pcl::io::savePCDFileBinary( "pointcloud_ideal.pcd", cloud );*/

	// point cloud with disturb edge, confidence threshold, and change to millimeter
	for( unsigned int idx = 0 ; idx < cloud.size(); ++idx )
	{

		// check validatioin of data, in depth range & confidence != 0
		if( m_rayconf_buffer[ 4 * idx + 3 ] != -1  && m_rayconf_buffer[ 4 * idx + 2 ] < 0 && temp_depth_buffer[ 3 * idx ] != 255 )
		{
			cloud[idx].x = m_rayconf_buffer[ 4 * idx + 0 ] * 1000;
			cloud[idx].y = m_rayconf_buffer[ 4 * idx + 1 ] * 1000;
			cloud[idx].z = m_rayconf_buffer[ 4 * idx + 2 ] * 1000;
		}
		else
		{
			cloud[idx].x = std::numeric_limits<float>::quiet_NaN();
			cloud[idx].y = std::numeric_limits<float>::quiet_NaN();
			cloud[idx].z = std::numeric_limits<float>::quiet_NaN();
		}

	}
	// TODO : TEMP START
	//	cout << "extract point cloud : " << common::Time::GetWallTime().Double() - time << endl;
	//	time = common::Time::GetWallTime().Double();
	// TEMP END

	// ******************************* //
	// add sensor noise to point cloud //
	// ******************************* //
	for( unsigned int idx = 0 ; idx < cloud.size(); ++idx )
	{
		if( !pcl::isFinite( cloud[idx] ) )
		{
			continue;
		}

		cloud[idx].z += m_noise.at<float>( idx / width, idx % width ) * 3;
	}

	// TODO : If you want to save the point cloud, just uncomment this part
	// ********************* //
	// save Point Cloud Data //
	// ********************* //
	/*
	boost::filesystem::create_directory( "only_snapshot/pcd" );
	// save pcd file from gazebo's sensor
	if(m_snapshot)
	{
		ss.clear();
		ss.str( "" );
		save_string.clear();
		m_save_file_number = m_save_count + m_current_file_number;
		ss << "only_snapshot/pcd/pointcloud_" << m_save_file_number << ".pcd";
		save_string = ss.str();
		std::cout << "save pcd = " << save_string << endl;
		pcl::io::savePCDFileBinary(save_string, cloud);
	}
	else
	{
		pcl::io::savePCDFileBinary( "pointcloud.pcd", cloud );
	}
	 */

	//pcl::io::savePCDFileBinary( "pointcloud_noise.pcd", cloud );

	// TODO : TEMP START
	//	cout << "add noise: " << common::Time::GetWallTime().Double() - time << endl;
	//	time = common::Time::GetWallTime().Double();
	// TEMP END

	// ****************** //
	// smooth point cloud //
	// ****************** //

	// get Gaussian kernel
	/*const int kernel_size = 11;
	const int k = ( kernel_size - 1 ) / 2;
	const double sigma = 4;

	cv::Mat gauss_kernel = gaussianMaskGenerator( kernel_size, sigma );
	*/

	// start blurring on z - direction
	pcl::PointCloud< pcl::PointXYZ > blurred_cloud;

	// Useing Fast Bilateral Filter
	pcl::FastBilateralFilterOMP< pcl::PointXYZ > fast_bilateral_filter;
	fast_bilateral_filter.setSigmaS( 2.5 );
	fast_bilateral_filter.setSigmaR( 5 );
	fast_bilateral_filter.setInputCloud( cloud.makeShared() );
	fast_bilateral_filter.filter( blurred_cloud );

	//	pcl::io::savePCDFileBinary( "pointcloud_noise_blur.pcd", blurred_cloud );

	// TODO : TEMP START
	//	cout << "blurring : " << common::Time::GetWallTime().Double() - time << endl;
	//	time = common::Time::GetWallTime().Double();
	// TEMP END


	// TEMP END

	/*for( int j = 0; j < height; j++ )
	{
		for( int i = 0; i < width; i++ )
		{
			if( !pcl::isFinite( cloud( i, j ) ) )
			{
				continue;
			}

			float norm_factor = 0.f;
			float sum = 0.f;

			for( int y = 0; y < kernel_size; y++ )
			{
				for( int x = 0; x < kernel_size; x++ )
				{
					if( ( i - k + x ) < 0 ||
						( j - k + y ) < 0 ||
						( i - k + x ) >= width ||
						( j - k + y ) >= height ||
						!pcl::isFinite( cloud( i - k + x, j - k + y ) ) ||
						abs( cloud( i, j ).z - cloud( i - k +x, j - k + y ).z ) > 10 )
					{
						continue;
					}

					sum += cloud( i - k + x, j - k + y ).z * gauss_kernel.at<float>( y, x );
					norm_factor += gauss_kernel.at<float>( y, x );
				}
			}

			blurred_cloud.at( i, j ).z = sum / norm_factor;
		}
	}

	//pcl::io::savePCDFileBinary( "pointcloud_noise_blur.pcd", blurred_cloud );*/

	// if it is in only_snapshot mode, we don't need to do pose estimation
	if(m_snapshot)
	{
		msgs::Request rethrow_event;
		rethrow_event.set_id( 2 );
		rethrow_event.set_request( "rethrow_in_evaluation_platform" );
		rethrow_event.set_data(std::to_string(m_save_count));
		m_rethrow_publisher_ptr->Publish(rethrow_event);
		m_save_count++;
		return;
	}
	// if it is in only_snapshot mode, we don't need to do pose estimation

	// *************************************** //
	// send sensor data through gazebo message //
	// *************************************** //
	while( !m_publisher_ptr->HasConnections() )
	{
		cout << "[DepthSensorPlugin] Waiting for connection..." << endl;
		gazebo::common::Time::MSleep( 500 );
	}

	if( m_publisher_ptr->HasConnections() )
	{
		if( m_use_ideal_segmentation )
		{
			// publish PointCloud
			pcl::msgs::PointCloudXYZL msgs_pointcloudxyzl;
			msgs_pointcloudxyzl.set_width( blurred_cloud.width );
			msgs_pointcloudxyzl.set_height( blurred_cloud.height );
			msgs_pointcloudxyzl.set_is_dense( blurred_cloud.is_dense );
			for( int j = 0; j < height; j++ )
			{
				for( int i = 0; i < width; i++ )
				{
					pcl::msgs::PointXYZL point_xyzl;
					point_xyzl.set_x( blurred_cloud( i, j ).x );
					point_xyzl.set_y( blurred_cloud( i, j ).y );
					point_xyzl.set_z( blurred_cloud( i, j ).z );
					point_xyzl.set_label( m_segment_buffer[ ( i + j * width ) * 3 ] );

					msgs_pointcloudxyzl.add_points()->CopyFrom( point_xyzl );
				}
			}
			cout << "Publishing PointCloud..." << endl;
			m_publisher_ptr->Publish( msgs_pointcloudxyzl );
		}
		else // not use ideal segmentation
		{
			// publish PointCloud
			pcl::msgs::PointCloud msgs_pointcloud;
			msgs_pointcloud.set_width( blurred_cloud.width );
			msgs_pointcloud.set_height( blurred_cloud.height );
			msgs_pointcloud.set_is_dense( blurred_cloud.is_dense );
			for( int j = 0; j < height; j++ )
			{
				for( int i = 0; i < width; i++ )
				{
					pcl::msgs::PointXYZ point_xyz;
					point_xyz.set_x( blurred_cloud( i, j ).x );
					point_xyz.set_y( blurred_cloud( i, j ).y );
					point_xyz.set_z( blurred_cloud( i, j ).z );
					msgs_pointcloud.add_points()->CopyFrom( point_xyz );
				}
			}
			cout << "Publishing PointCloud..." << endl;
			m_publisher_ptr->Publish( msgs_pointcloud );
		}
	}
	else
	{
		cout << "Has no connections !..." << endl;
	}

	// TODO : TEMP START
	//	cout << "send message : " << common::Time::GetWallTime().Double() - time << endl;
	//	time = common::Time::GetWallTime().Double();
	// TEMP END

	// free memory
	delete [] temp_depth_buffer;
}

void DepthSensorPlugin::_disturbOcclusionEdge( unsigned char *_depth_buffer )
{
	// get sensor info
	int width = m_rgb_rt->getWidth();
	int height = m_rgb_rt->getHeight();

	// get a copy of depth buffer
	unsigned char *temp_buffer = new unsigned char[ width * height * 3 ];
	memcpy( temp_buffer, _depth_buffer, width * height * 3 * sizeof( unsigned char ) );

	// generate pelin noise
	vector< float > noise_5 = perlinNoise( width, height, 5 );	// amplitude about -2.5~2.5
	vector< float > noise_10 = perlinNoise( width, height, 10 );	// amplitude about -5 ~ 5
	vector< float > noise_20 = perlinNoise( width, height, 20 );	// amplitude about -10 ~ 10

	vector< float > noise( width * height );

	for( int j = 0; j < height; j++ )
	{
		for( int i = 0; i < width; i++ )
		{
			noise[ i + j * width ] =	noise_5[ i + j * width ] +
										noise_10[ i + j * width] +
										noise_20[ i + j * width];
		}
	}

	cv::normalize( noise, noise, -10, 10, cv::NORM_MINMAX );

	// generate elements(diamond shape ) for each size
	int max_size = 20;
	vector< vector< math::Vector2i > > elements;
	for( int cur_size = 0; cur_size <= max_size; cur_size++ )
	{
		vector< math::Vector2i > cur_elements;

		for( int x = cur_size; x > 0; x-- )
		{
			cur_elements.push_back( math::Vector2i( x, 0 ) );
			cur_elements.push_back( math::Vector2i( -x, 0 ) );
		}

		for( int y = 1; y <= cur_size; y++ )
		{
			cur_elements.push_back( math::Vector2i( 0, y ) );
			cur_elements.push_back( math::Vector2i( 0, -y ) );
		}

		for( int y = 1; y < cur_size; y++ )
		{
			for( int x = cur_size - y; x > 0; x-- )
			{
				cur_elements.push_back( math::Vector2i( x, y ) );
				cur_elements.push_back( math::Vector2i( -x, y ) );
				cur_elements.push_back( math::Vector2i( x, -y ) );
				cur_elements.push_back( math::Vector2i( -x, -y ) );
			}
		}

		elements.push_back( cur_elements );
	}

	// erosion
	for( int j = 1; j < height - 1; j++ )
	{
		for( int i = 1; i < width - 1; i++ )
		{
			int erosion_size = abs( (int)noise[ i + j * width] );

			// check if is NaN point && do erosion if noise >= 1 ( erosion size )
			if(	temp_buffer[ 3 * ( i + j * width ) ] == 255 &&
				erosion_size >= 1 &&
				(	temp_buffer[ 3 * ( i - 1+ j * width ) ] != 255 ||
					temp_buffer[ 3 * ( i + 1+ j * width ) ] != 255 ||
					temp_buffer[ 3 * ( i + ( j - 1 ) * width ) ] != 255 ||
					temp_buffer[ 3 * ( i + ( j + 1 ) * width ) ] != 255 ) )
			{
				// TODO : use template element to do erosion
				const vector< math::Vector2i > &cur_element = elements[ erosion_size ];

				for( uint idx = 0; idx < cur_element.size(); idx++ )
				{
					int target_x = i + cur_element[ idx ].x;
					int target_y = j + cur_element[ idx ].y;
					if( target_x < width &&
						target_x >= 0 &&
						target_y < height &&
						target_y >= 0 )
					{
						_depth_buffer[ 3 * ( target_x + target_y * width ) ] = 255;
						_depth_buffer[ 3 * ( target_x + target_y * width ) + 1 ] = 255;
						_depth_buffer[ 3 * ( target_x + target_y * width ) + 2 ] = 255;
					}

				}

			}
		}
	}

	// free memory
	delete [] temp_buffer;
}

// _mask_size must be odd number and positive
cv::Mat DepthSensorPlugin::gaussianMaskGenerator( int _mask_size, float _sigma )
{
	cv::Mat mask( _mask_size, _mask_size, CV_32FC1 );
	int k = ( _mask_size - 1 ) / 2;

	float itpss = 1 / ( 2 * M_PI * _sigma * _sigma );
	float den = 2 * _sigma * _sigma;

	for( int j = 0; j < _mask_size; j++ )
	{
		for( int i = 0; i < _mask_size; i++ )
		{
			float num = ( i - k ) * ( i - k ) + ( j - k ) * ( j - k );
			mask.at<float>( j, i ) = itpss * exp( -num / den );
		}
	}

	// print out kernel
	/*	for( int j = 0; j < _mask_size; j++ )
	{
		for( int i = 0; i < _mask_size; i++ )
		{
			std::cout << mask.at<float>( j, i ) << "\t";
		}
		std::cout << std::endl;
	}*/

	return mask.clone();
}

void DepthSensorPlugin::_setupIdealSegmentation()
{
	// ************************* //
	// get necessary information //
	// ************************* //
	// get camera info
	unsigned int cam_w = m_camera_sensor->GetImageWidth();
	unsigned int cam_h = m_camera_sensor->GetImageHeight();
	std::string camera_name = m_camera->GetName();

	// ******************** //
	// setup render texture //
	// ******************** //
	// create render texture
	Ogre::TexturePtr rtt_texture;
	rtt_texture = Ogre::TextureManager::getSingleton().createManual(	"Idea_Segmentation_" + camera_name,
																		Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
																		Ogre::TEX_TYPE_2D,
																		cam_w,
																		cam_h,
																		0,
																		Ogre::PF_BYTE_RGB,
																		Ogre::TU_RENDERTARGET );
	m_segment_rt = rtt_texture->getBuffer()->getRenderTarget();

	// setup render texture
	m_segment_rt -> addViewport( m_ogre_camera );
	m_segment_rt -> getViewport( 0 )->setClearEveryFrame( true );
	// background set to ( 0, 0, 0 ), means no object at that pixel
	m_segment_rt -> getViewport( 0 )->setBackgroundColour( Ogre::ColourValue( 0, 0, 0 ) );
	m_segment_rt -> getViewport( 0 )->setOverlaysEnabled( false );

	// allocate memory for rgb frame buffer
	m_segment_buffer = new unsigned char[ cam_w * cam_h * 3 ];

	// create rgb render target listener
	m_segment_rt_listener = new SegmentRTListener( m_scene, m_scene_mgr, m_segment_rt, m_segment_buffer );

	m_segment_rt -> addListener( m_segment_rt_listener );
}

void DepthSensorPlugin::_getIdealSegmentation()
{
	int width = m_camera->GetImageWidth();
	int height = m_camera->GetImageHeight();

	// save image;
	common::Image img;
	cout << "SetFromData" << endl;
	img.SetFromData( m_segment_buffer, width, height, common::Image::RGB_INT8 );
	cout << "saving image" << endl;
	img.SavePNG( "test.png" );
}



		// TEMP START			( !!! kind of useful !!! )
	/*
		// to find out the relation ship between gazebo's material and ogre's material
		unsigned int num_visual = m_scene->GetVisualCount();
		cout << "num of visual instance:" << num_visual << endl;
		unsigned int temp_count = 0;
		// go through every visual in rendering::Scene
		for( unsigned int i = 1; i < num_visual+10; i++ )	// skip the first one
		{
			rendering::VisualPtr cur_visual = m_scene->GetVisual( i );

			if( cur_visual )
			{
				cout << "== " << i << " ==" << endl;
				cout << "\tvisual name:" << cur_visual->GetName() << "\tvisible:" << cur_visual->GetVisible() << endl;
				cout << "\tvisual material name:" << cur_visual->GetMaterialName() << endl;

				Ogre::SceneNode *scene_node = cur_visual->GetSceneNode();
				for( int j = 0; j < scene_node->numAttachedObjects(); j++ )
				{
					Ogre::MovableObject *obj = scene_node->getAttachedObject( j );

					if( dynamic_cast<Ogre::Entity*>( obj ) )
					{
						cout << "\t\togre entity name:" << dynamic_cast<Ogre::Entity*>( obj )->getName() << endl;
						cout << "\t\togre entity material name:" << "doesn't exist";
						cout << "\tvisible:" << dynamic_cast<Ogre::Entity*>( obj )->getVisible() << endl;
					}
					else if (dynamic_cast<Ogre::SimpleRenderable*>(obj))
					{
						cout << "\t\togre simple renderable name:" << dynamic_cast<Ogre::SimpleRenderable*>(obj)->getName() << endl;
						cout << "\t\togre simple renderable material name:" << ((Ogre::SimpleRenderable*)obj)->getMaterial()->getName();
						cout << "\tvisible:" << dynamic_cast<Ogre::SimpleRenderable*>(obj)->getVisible() << endl;
					}
				}


				// Apply material to all child scene nodes
				for (unsigned int i = 0; i < scene_node->numChildren(); ++i)
				{
					Ogre::SceneNode *sn = (Ogre::SceneNode*)(scene_node->getChild(i));
					for (int j = 0; j < sn->numAttachedObjects(); j++)
					{
						Ogre::MovableObject *obj = sn->getAttachedObject(j);

						if (dynamic_cast<Ogre::Entity*>(obj))
						{
							cout << "\t\t\togre entity name:" << dynamic_cast<Ogre::Entity*>( obj )->getName() << endl;
							cout << "\t\t\togre entity material name:" << "doesn't exist";
							cout << "\tvisible:" << dynamic_cast<Ogre::Entity*>( obj )->getVisible() << endl;
						}
						else
						{
							cout << "\t\t\togre simple renderable name:" << dynamic_cast<Ogre::SimpleRenderable*>(obj)->getName() << endl;
							cout << "\t\t\togre simple renderable material name:" << ((Ogre::SimpleRenderable*)obj)->getMaterial()->getName();
							cout << "\tvisible:" << dynamic_cast<Ogre::SimpleRenderable*>(obj)->getVisible() << endl;
						}
					}
				}
			}
			else
			{
				temp_count++;
			}
		}
		cout << "num of visuals : " << num_visual << endl;
		cout << "not available visual : " << temp_count << endl;

	 */

		// TEMP END


