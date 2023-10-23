// -- BEGIN LICENSE BLOCK ----------------------------------------------
/*!
*  Copyright (C) 2023, SICK AG, Waldkirch, Germany
*  Copyright (C) 2023, FZI Forschungszentrum Informatik, Karlsruhe, Germany
*
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.

*/
// -- END LICENSE BLOCK ------------------------------------------------

//----------------------------------------------------------------------
/*!\file
 *
 * \author  Marvin Große Besselmann <grosse@fzi.de>
 * \author  Stefan Scherzinger <scherzin@fzi.de>
 * \date    2023-08-01
 */
//----------------------------------------------------------------------

#include <sick_safevisionary_base/SafeVisionaryData.h>
#include <sick_safevisionary_driver/compound_publisher.h>

CompoundPublisher::CompoundPublisher()
  : priv_nh_("~/")
{
  camera_info_pub_ = priv_nh_.advertise<sensor_msgs::CameraInfo>("camera_info", 1);
  pointcloud_pub_  = priv_nh_.advertise<sensor_msgs::PointCloud2>("points", 1);
  imu_pub_         = priv_nh_.advertise<sensor_msgs::Imu>("imu_data", 1);
  io_pub_          = priv_nh_.advertise<sick_safevisionary_msgs::CameraIO>("camera_io", 1);
  roi_pub_         = priv_nh_.advertise<sick_safevisionary_msgs::ROIArray>("region_of_interest", 1);
  field_pub_ = priv_nh_.advertise<sick_safevisionary_msgs::FieldInformationArray>("fields", 1);
  device_status_pub_ =
    priv_nh_.advertise<sick_safevisionary_msgs::DeviceStatus>("device_status", 1);

  image_transport::ImageTransport image_transport(priv_nh_);
  depth_pub_     = image_transport.advertise("depth", 1);
  intensity_pub_ = image_transport.advertise("intensity", 1);
  state_pub_     = image_transport.advertise("state", 1);
}


void CompoundPublisher::publish(const std_msgs::Header& header,
                                visionary::SafeVisionaryData& frame_data)
{
  if (camera_info_pub_.getNumSubscribers() > 0)
  {
    publishCameraInfo(header, frame_data);
  }
  if (pointcloud_pub_.getNumSubscribers() > 0)
  {
    publishPointCloud(header, frame_data);
  }
  if (depth_pub_.getNumSubscribers() > 0)
  {
    publishDepthImage(header, frame_data);
  }
  if (intensity_pub_.getNumSubscribers() > 0)
  {
    publishIntensityImage(header, frame_data);
  }
  if (state_pub_.getNumSubscribers() > 0)
  {
    publishStateMap(header, frame_data);
  }
  if (imu_pub_.getNumSubscribers() > 0)
  {
    publishIMUData(header, frame_data);
  }
  if (device_status_pub_.getNumSubscribers() > 0)
  {
    publishDeviceStatus(header, frame_data);
  }
  if (io_pub_.getNumSubscribers() > 0)
  {
    publishIOs(header, frame_data);
  }
  if (roi_pub_.getNumSubscribers() > 0)
  {
    publishROI(header, frame_data);
  }
  if (field_pub_.getNumSubscribers() > 0)
  {
    publishFieldInformation(header, frame_data);
  }
}

void CompoundPublisher::publishCameraInfo(const std_msgs::Header& header,
                                          const visionary::SafeVisionaryData& frame_data)
{
  sensor_msgs::CameraInfo camera_info;
  camera_info.header = header;

  camera_info.height = frame_data.getHeight();
  camera_info.width  = frame_data.getWidth();
  camera_info.D      = std::vector<double>(5, 0);
  camera_info.D[0]   = frame_data.getCameraParameters().k1;
  camera_info.D[1]   = frame_data.getCameraParameters().k2;
  camera_info.D[2]   = frame_data.getCameraParameters().p1;
  camera_info.D[3]   = frame_data.getCameraParameters().p2;
  camera_info.D[4]   = frame_data.getCameraParameters().k3;

  camera_info.K[0] = frame_data.getCameraParameters().fx;
  camera_info.K[2] = frame_data.getCameraParameters().cx;
  camera_info.K[4] = frame_data.getCameraParameters().fy;
  camera_info.K[5] = frame_data.getCameraParameters().cy;
  camera_info.K[8] = 1;
  // TODO add missing parameter in Projection Matrix
  camera_info_pub_.publish(camera_info);
}

void CompoundPublisher::publishPointCloud(const std_msgs::Header& header,
                                          visionary::SafeVisionaryData& frame_data)
{
  sensor_msgs::PointCloud2::Ptr cloud_msg(new sensor_msgs::PointCloud2);
  cloud_msg->header       = header;
  cloud_msg->height       = frame_data.getHeight();
  cloud_msg->width        = frame_data.getWidth();
  cloud_msg->is_dense     = false;
  cloud_msg->is_bigendian = false;

  cloud_msg->fields.resize(4);
  cloud_msg->fields[0].name = "x";
  cloud_msg->fields[1].name = "y";
  cloud_msg->fields[2].name = "z";
  cloud_msg->fields[3].name = "intensity";

  int offset = 0;
  for (size_t i = 0; i < 3; ++i)
  {
    cloud_msg->fields[i].offset   = offset;
    cloud_msg->fields[i].datatype = int(sensor_msgs::PointField::FLOAT32);
    cloud_msg->fields[i].count    = 1;
    offset += sizeof(float);
  }

  cloud_msg->fields[3].offset   = offset;
  cloud_msg->fields[3].datatype = int(sensor_msgs::PointField::UINT16);
  cloud_msg->fields[3].count    = 1;
  offset += sizeof(uint16_t);

  cloud_msg->point_step = offset;
  cloud_msg->row_step   = cloud_msg->point_step * cloud_msg->width;
  cloud_msg->data.resize(cloud_msg->height * cloud_msg->row_step);

  std::vector<visionary::PointXYZ> point_vec;
  frame_data.generatePointCloud(point_vec);
  frame_data.transformPointCloud(point_vec);

  if (frame_data.getIntensityMap().size() != point_vec.size())
  {
    ROS_INFO_STREAM("Missmatch point and intensity data.");
    return;
  }

  std::vector<uint16_t>::const_iterator intensity_it        = frame_data.getIntensityMap().begin();
  std::vector<visionary::PointXYZ>::const_iterator point_it = point_vec.begin();
  // TODO check if both vector sizes align

  for (size_t i = 0; i < point_vec.size(); ++i, ++intensity_it, ++point_it)
  {
    memcpy(&cloud_msg->data[i * cloud_msg->point_step + cloud_msg->fields[0].offset],
           &*point_it,
           sizeof(visionary::PointXYZ));
    memcpy(&cloud_msg->data[i * cloud_msg->point_step + cloud_msg->fields[3].offset],
           &*intensity_it,
           sizeof(uint16_t));
  }
  pointcloud_pub_.publish(cloud_msg);
}

void CompoundPublisher::publishIMUData(const std_msgs::Header& header,
                                       const visionary::SafeVisionaryData& frame_data)
{
  sensor_msgs::Imu imu_msg;
  imu_msg.header                = header;
  imu_msg.angular_velocity.x    = frame_data.getIMUData().angularVelocity.X;
  imu_msg.angular_velocity.y    = frame_data.getIMUData().angularVelocity.Y;
  imu_msg.angular_velocity.z    = frame_data.getIMUData().angularVelocity.Z;
  imu_msg.linear_acceleration.x = frame_data.getIMUData().acceleration.X;
  imu_msg.linear_acceleration.y = frame_data.getIMUData().acceleration.Y;
  imu_msg.linear_acceleration.z = frame_data.getIMUData().acceleration.Z;
  imu_msg.orientation.x         = frame_data.getIMUData().orientation.X;
  imu_msg.orientation.y         = frame_data.getIMUData().orientation.Y;
  imu_msg.orientation.z         = frame_data.getIMUData().orientation.Z;
  imu_msg.orientation.w         = frame_data.getIMUData().orientation.W;
  imu_pub_.publish(imu_msg);
}

void CompoundPublisher::publishDepthImage(const std_msgs::Header& header,
                                          const visionary::SafeVisionaryData& frame_data)
{
  depth_pub_.publish(Vec16ToImage(header, frame_data, frame_data.getDistanceMap()));
}

void CompoundPublisher::publishIntensityImage(const std_msgs::Header& header,
                                              const visionary::SafeVisionaryData& frame_data)
{
  intensity_pub_.publish(Vec16ToImage(header, frame_data, frame_data.getIntensityMap()));
}

void CompoundPublisher::publishStateMap(const std_msgs::Header& header,
                                        const visionary::SafeVisionaryData& frame_data)
{
  state_pub_.publish(Vec8ToImage(header, frame_data, frame_data.getStateMap()));
}

sensor_msgs::ImagePtr
CompoundPublisher::Vec16ToImage(const std_msgs::Header& header,
                                const visionary::SafeVisionaryData& frame_data,
                                std::vector<uint16_t> vec)
{
  cv::Mat image = cv::Mat(frame_data.getHeight(), frame_data.getWidth(), CV_16UC1);
  std::memcpy(image.data, vec.data(), vec.size() * sizeof(uint16_t));
  return cv_bridge::CvImage(header, sensor_msgs::image_encodings::TYPE_16UC1, image).toImageMsg();
}
sensor_msgs::ImagePtr CompoundPublisher::Vec8ToImage(const std_msgs::Header& header,
                                                     const visionary::SafeVisionaryData& frame_data,
                                                     std::vector<uint8_t> vec)
{
  cv::Mat image = cv::Mat(frame_data.getHeight(), frame_data.getWidth(), CV_8UC1);
  std::memcpy(image.data, vec.data(), vec.size() * sizeof(uint8_t));
  return cv_bridge::CvImage(header, sensor_msgs::image_encodings::TYPE_8UC1, image).toImageMsg();
}


void CompoundPublisher::publishDeviceStatus(const std_msgs::Header& header,
                                            const visionary::SafeVisionaryData& frame_data)
{
  sick_safevisionary_msgs::DeviceStatus device_status_msg;
  device_status_msg.header = header;
  device_status_msg.status = static_cast<uint8_t>(frame_data.getDeviceStatus());
  device_status_msg.general_status.application_error =
    frame_data.getDeviceStatusData().generalStatus.applicationError;
  device_status_msg.general_status.contamination_error =
    frame_data.getDeviceStatusData().generalStatus.contaminationError;
  device_status_msg.general_status.contamination_warning =
    frame_data.getDeviceStatusData().generalStatus.contaminationWarning;
  device_status_msg.general_status.dead_zone_detection =
    frame_data.getDeviceStatusData().generalStatus.deadZoneDetection;
  device_status_msg.general_status.device_error =
    frame_data.getDeviceStatusData().generalStatus.deviceError;
  device_status_msg.general_status.temperature_warning =
    frame_data.getDeviceStatusData().generalStatus.temperatureWarning;
  device_status_msg.general_status.run_mode_active =
    frame_data.getDeviceStatusData().generalStatus.runModeActive;
  device_status_msg.general_status.wait_for_cluster =
    frame_data.getDeviceStatusData().generalStatus.waitForCluster;
  device_status_msg.general_status.wait_for_input =
    frame_data.getDeviceStatusData().generalStatus.waitForInput;
  device_status_msg.COP_non_safety_related = frame_data.getDeviceStatusData().COPNonSaftyRelated;
  device_status_msg.COP_safety_related     = frame_data.getDeviceStatusData().COPSaftyRelated;
  device_status_msg.COP_reset_required     = frame_data.getDeviceStatusData().COPResetRequired;
  device_status_msg.active_monitoring_case.monitoring_case_1 =
    frame_data.getDeviceStatusData().activeMonitoringCase.currentCaseNumberMonitoringCase1;
  device_status_msg.active_monitoring_case.monitoring_case_2 =
    frame_data.getDeviceStatusData().activeMonitoringCase.currentCaseNumberMonitoringCase2;
  device_status_msg.active_monitoring_case.monitoring_case_3 =
    frame_data.getDeviceStatusData().activeMonitoringCase.currentCaseNumberMonitoringCase3;
  device_status_msg.active_monitoring_case.monitoring_case_4 =
    frame_data.getDeviceStatusData().activeMonitoringCase.currentCaseNumberMonitoringCase4;
  device_status_msg.contamination_level = frame_data.getDeviceStatusData().contaminationLevel;
  device_status_pub_.publish(device_status_msg);
}

void CompoundPublisher::publishIOs(const std_msgs::Header& header,
                                   const visionary::SafeVisionaryData& frame_data)
{
  sick_safevisionary_msgs::CameraIO camera_io_msg;
  camera_io_msg.header = header;
  camera_io_msg.configured.pin_5 =
    frame_data.getLocalIOData().universalIOConfigured.configuredUniIOPin5;
  camera_io_msg.configured.pin_6 =
    frame_data.getLocalIOData().universalIOConfigured.configuredUniIOPin6;
  camera_io_msg.configured.pin_7 =
    frame_data.getLocalIOData().universalIOConfigured.configuredUniIOPin7;
  camera_io_msg.configured.pin_8 =
    frame_data.getLocalIOData().universalIOConfigured.configuredUniIOPin8;
  camera_io_msg.direction.pin_5 =
    frame_data.getLocalIOData().universalIODirection.directionValueUniIOPin5;
  camera_io_msg.direction.pin_6 =
    frame_data.getLocalIOData().universalIODirection.directionValueUniIOPin6;
  camera_io_msg.direction.pin_7 =
    frame_data.getLocalIOData().universalIODirection.directionValueUniIOPin7;
  camera_io_msg.direction.pin_8 =
    frame_data.getLocalIOData().universalIODirection.directionValueUniIOPin8;
  camera_io_msg.input_values.pin_5 =
    frame_data.getLocalIOData().universalIOInputValue.logicalValueUniIOPin5;
  camera_io_msg.input_values.pin_6 =
    frame_data.getLocalIOData().universalIOInputValue.logicalValueUniIOPin6;
  camera_io_msg.input_values.pin_7 =
    frame_data.getLocalIOData().universalIOInputValue.logicalValueUniIOPin7;
  camera_io_msg.input_values.pin_8 =
    frame_data.getLocalIOData().universalIOInputValue.logicalValueUniIOPin8;
  camera_io_msg.output_values.pin_5 =
    frame_data.getLocalIOData().universalIOOutputValue.localOutput1Pin5;
  camera_io_msg.output_values.pin_6 =
    frame_data.getLocalIOData().universalIOOutputValue.localOutput2Pin6;
  camera_io_msg.output_values.pin_7 =
    frame_data.getLocalIOData().universalIOOutputValue.localOutput3Pin7;
  camera_io_msg.output_values.pin_8 =
    frame_data.getLocalIOData().universalIOOutputValue.localOutput4Pin8;
  camera_io_msg.ossds_state.OSSD1A  = frame_data.getLocalIOData().ossdsState.stateOSSD1A;
  camera_io_msg.ossds_state.OSSD1B  = frame_data.getLocalIOData().ossdsState.stateOSSD1B;
  camera_io_msg.ossds_state.OSSD2A  = frame_data.getLocalIOData().ossdsState.stateOSSD2A;
  camera_io_msg.ossds_state.OSSD2B  = frame_data.getLocalIOData().ossdsState.stateOSSD2B;
  camera_io_msg.ossds_dyn_count     = frame_data.getLocalIOData().ossdsDynCount;
  camera_io_msg.ossds_crc           = frame_data.getLocalIOData().ossdsCRC;
  camera_io_msg.ossds_io_status     = frame_data.getLocalIOData().ossdsIOStatus;
  camera_io_msg.dynamic_speed_a     = frame_data.getLocalIOData().dynamicSpeedA;
  camera_io_msg.dynamic_speed_b     = frame_data.getLocalIOData().dynamicSpeedB;
  camera_io_msg.dynamic_valid_flags = frame_data.getLocalIOData().DynamicValidFlags;
  io_pub_.publish(camera_io_msg);
}

void CompoundPublisher::publishROI(const std_msgs::Header& header,
                                   const visionary::SafeVisionaryData& frame_data)
{
  sick_safevisionary_msgs::ROIArray roi_array_msg;
  roi_array_msg.header = header;
  for (auto& roi : frame_data.getRoiData().roiData)
  {
    sick_safevisionary_msgs::ROI roi_msg;
    roi_msg.id                         = roi.id;
    roi_msg.distance_value             = roi.distanceValue;
    roi_msg.result_data.distance_safe  = roi.result.distanceSafe;
    roi_msg.result_data.distance_valid = roi.result.distanceValid;
    roi_msg.result_data.result_safe    = roi.result.resultSafe;
    roi_msg.result_data.result_valid   = roi.result.resultValid;
    roi_msg.result_data.task_result    = roi.result.taskResult;
    roi_msg.safety_data.invalid_due_to_invalid_pixels =
      roi.safetyRelatedData.tMembers.invalidDueToInvalidPixels;
    roi_msg.safety_data.invalid_due_to_variance =
      roi.safetyRelatedData.tMembers.invalidDueToVariance;
    roi_msg.safety_data.invalid_due_to_overexposure =
      roi.safetyRelatedData.tMembers.invalidDueToOverexposure;
    roi_msg.safety_data.invalid_due_to_underexposure =
      roi.safetyRelatedData.tMembers.invalidDueToUnderexposure;
    roi_msg.safety_data.invalid_due_to_temporal_variance =
      roi.safetyRelatedData.tMembers.invalidDueToTemporalVariance;
    roi_msg.safety_data.invalid_due_to_outside_of_measurement_range =
      roi.safetyRelatedData.tMembers.invalidDueToOutsideOfMeasurementRange;
    roi_msg.safety_data.invalid_due_to_retro_reflector_interference =
      roi.safetyRelatedData.tMembers.invalidDueToRetroReflectorInterference;
    roi_msg.safety_data.contamination_error = roi.safetyRelatedData.tMembers.contaminationError;
    roi_msg.safety_data.quality_class       = roi.safetyRelatedData.tMembers.qualityClass;
    roi_msg.safety_data.slot_active         = roi.safetyRelatedData.tMembers.slotActive;
    roi_array_msg.rois.push_back(roi_msg);
  }
  roi_pub_.publish(roi_array_msg);
}

void CompoundPublisher::publishFieldInformation(const std_msgs::Header& header,
                                                const visionary::SafeVisionaryData& frame_data)
{
  sick_safevisionary_msgs::FieldInformationArray field_array_msg;
  field_array_msg.header = header;
  for (auto& field : frame_data.getFieldInformationData().fieldInformation)
  {
    sick_safevisionary_msgs::FieldInformation field_msg;
    field_msg.field_id     = field.fieldID;
    field_msg.field_set_id = field.fieldSetID;
    field_msg.field_active = field.fieldActive;
    field_msg.field_result = field.fieldResult;
    field_msg.eval_method  = field.evalMethod;
    field_array_msg.fields.push_back(field_msg);
  }
  field_pub_.publish(field_array_msg);
}
