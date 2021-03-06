/*
 * depth_sensor_plugin.h
 *
 *  Created on: Feb 15, 2017
 *      Author: kevin
 */

#ifndef DEPTH_SENSOR_PLUGIN_H_
#define DEPTH_SENSOR_PLUGIN_H_

#include <gazebo/gazebo.hh>
#include <gazebo/sensors/sensors.hh>
#include <gazebo/rendering/rendering.hh>

#include <dirent.h>
#include <boost/filesystem/operations.hpp>

#include <OGRE/Ogre.h>

#include <opencv2/core/core.hpp>

#include "ShadowSettings.h"

#include "RGBRTListener.h"
#include "DepthRTListener.h"
#include "RayConfRTListener.h"
#include "SegmentRTListener.h"

using namespace gazebo;

typedef const boost::shared_ptr<const gazebo::msgs::Request > ConstMsgsRequestPtr;

class DepthSensorPlugin : public SensorPlugin
{
public:
	// constructor
	DepthSensorPlugin();
	// destructor
	virtual ~DepthSensorPlugin();

	/// \brief Load the sensor plugin.
	/// \param[in] _sensor Pointer to the sensor that loaded this plugin.
	/// \param[in] _sdf SDF element that describes the plugin.
	virtual void Load( sensors::SensorPtr _sensor, sdf::ElementPtr _sdf );

private:
	/// \brief Callback that recieves the camera's update signal.
	virtual void _onUpdatedCallback();

	void _onlySnapshot( ConstMsgsRequestPtr &_msgs );

	void _takePicture( ConstMsgsRequestPtr &_msgs );

	void _sendSensorPose( ConstMsgsRequestPtr &_msgs );

	// callback that recieves the preRender event
	//virtual void _preRenderCallback();

	// load cg plugin
	void _loadPlugins();
	// add resources for depth shadow map
	void _addResources();

	// setup depth sensor
	void _setupDepthSensor();

	// check the number of files in a directory
	void _check_file_number();

	// save sensor data
	void _saveSensorData();

	// prepare sensor noise
	void _prepareSensorNoise();

	// disturb occlusion edge
	void _disturbOcclusionEdge( unsigned char *_depth_buffer );

	// gaussian mask generator for blurring process
	cv::Mat gaussianMaskGenerator( int _mask_size, float _sigma );


	// setup ideal segmentation functionality
	void _setupIdealSegmentation();

	// get ideal segmenation
	void _getIdealSegmentation();

public:
private:
	// gazebo CameraSensor
	sensors::CameraSensorPtr m_camera_sensor;

	// gazebo CameraPtr
	rendering::CameraPtr m_camera;

	// get gazebo::Scene that this sensor is in
	rendering::ScenePtr m_scene;

	/// \brief Connection that maintains a link between the camera's
	/// updated signal and the OnUpdate callback.
	event::ConnectionPtr m_update_connection;

	// Connection that maintains a link between the render event and DepthSensorPlugin::render() callback
	//event::ConnectionPtr m_render_connection;

	// Ogre SceneManager
	Ogre::SceneManager *m_scene_mgr;
	// Ogre Camera
	Ogre::Camera *m_ogre_camera;

	// instance to save gazebo's shadow settings
	ShadowSettings m_shadow_settings;
	// this vector contain lights being turned off by this class
	vector<Ogre::Light *>	m_turned_off_lights;
	// this vector contain MovableObject being hidden by depth RT listener
	vector< Ogre::MovableObject * > m_turned_off_mobj;
	// this vcetor contain MovableObject being cloned for depth simulator
	vector< Ogre::Entity * > m_cloned_entity;



	// distance between IR camera & IR projector
	Ogre::Real m_base_dist;

	//
	const std::string SENSOR_IR_PROJECTOR_NAME_PREFIX;

	// RGB render texture
	Ogre::RenderTexture *m_rgb_rt;
	// RGB render texture listener
	RGBRTListener *m_rgb_rt_listener;
	// rgb frame buffer
	unsigned char *m_rgb_buffer;

	// depth render texture
	Ogre::RenderTexture *m_depth_rt;
	// depth render texture listener
	DepthRTListener *m_depth_rt_listener;
	// depth frame buffer
	float *m_depth_buffer;

	// rayconf render texture
	Ogre::RenderTexture *m_rayconf_rt;
	// rayconf render texture listener
	RayConfRTListener *m_rayconf_rt_listener;
	// rayconf frame buffer
	float *m_rayconf_buffer;

	// sensor noise
	cv::Mat m_noise;

	// transport::Node
	transport::NodePtr m_node_ptr;

	// transport::Publisher for transferring PointCloud of this Sensor
	transport::PublisherPtr m_publisher_ptr;

	// transport::Publisher for re-throwing objects in evaluation platform
	transport::PublisherPtr m_rethrow_publisher_ptr;

	// Subscribe for "~/evaluation_platform/take_picture_request"
	transport::SubscriberPtr m_subscriber_ptr;

	// Subscribe for "~/evaluation_platform/only_snapshot"
	transport::SubscriberPtr m_snapshot_subscriber_ptr;


	// take picture switch
	bool m_take_picture;
	// snapshot_switch
	bool m_snapshot = false;

	// parameters for only snapshot
	int m_save_count = 1;
	int m_total_snapshot;
	std::ostringstream ss;
	std::string save_string;
	int m_save_file_number;
	// for checking file number
	int m_current_file_number = 0;

	float m_msgs[19] = {0};

	// ********************* //
	// for idea segmentation //
	// ********************* //
	// segment render texture
	Ogre::RenderTexture *m_segment_rt;
	// rayconf render texture listener
	SegmentRTListener *m_segment_rt_listener;
	// rayconf frame buffer
	unsigned char *m_segment_buffer;

	// ********** //
	// parameters //
	// ********** //
	bool m_use_ideal_segmentation;
};

// Register this plugin with the simulator
GZ_REGISTER_SENSOR_PLUGIN( DepthSensorPlugin )

#endif /* DEPTH_SENSOR_PLUGIN_H_ */
