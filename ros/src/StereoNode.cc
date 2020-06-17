#include "StereoNode.h"

#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>

int main(int argc, char** argv)
{
  ros::init(argc, argv, "Stereo");
  ros::start();

  if (argc > 1)
  {
    ROS_WARN("Arguments supplied via command line are neglected.");
  }

  ros::NodeHandle node_handle;
  image_transport::ImageTransport image_transport(node_handle);

  // initilaize
  StereoNode node(ORB_SLAM2::System::STEREO, node_handle, image_transport);

  ros::spin();

  return 0;
}


StereoNode::StereoNode(const ORB_SLAM2::System::eSensor sensor, ros::NodeHandle& node_handle,
                       image_transport::ImageTransport& image_transport)
  : Node(sensor, node_handle, image_transport)
{
  left_sub_ = new message_filters::Subscriber<sensor_msgs::Image>(
      node_handle, "image_left/image_color_rect", 1);
  right_sub_ = new message_filters::Subscriber<sensor_msgs::Image>(
      node_handle, "image_right/image_color_rect", 1);

  sync_ = new message_filters::Synchronizer<sync_pol>(sync_pol(10), *left_sub_, *right_sub_);
  sync_->registerCallback(boost::bind(&StereoNode::ImageCallback, this, _1, _2));

  dpt_sub_ =
      image_transport.subscribe("depth/image_raw", 1, &StereoNode::depthImageCallback, this);
}


StereoNode::~StereoNode()
{
  delete left_sub_;
  delete right_sub_;
  delete sync_;
}


void StereoNode::ImageCallback(const sensor_msgs::ImageConstPtr& msgLeft,
                               const sensor_msgs::ImageConstPtr& msgRight)
{
  cv_bridge::CvImageConstPtr cv_ptrLeft;
  try
  {
    cv_ptrLeft = cv_bridge::toCvShare(msgLeft);
  }
  catch (cv_bridge::Exception& e)
  {
    ROS_ERROR("cv_bridge exception: %s", e.what());
    return;
  }

  cv_bridge::CvImageConstPtr cv_ptrRight;
  try
  {
    cv_ptrRight = cv_bridge::toCvShare(msgRight);
  }
  catch (cv_bridge::Exception& e)
  {
    ROS_ERROR("cv_bridge exception: %s", e.what());
    return;
  }

  current_frame_time_ = msgLeft->header.stamp;

  orb_slam_->TrackStereo(cv_ptrLeft->image, cv_ptrRight->image,
                         cv_ptrLeft->header.stamp.toSec());

  Update();
}

void StereoNode::depthImageCallback(const sensor_msgs::ImageConstPtr& dpt_msg)
{
  cv_bridge::CvImageConstPtr cv_ptr_dpt;
  try
  {
    cv_ptr_dpt = cv_bridge::toCvShare(dpt_msg);
  }
  catch (cv_bridge::Exception& e)
  {
    ROS_ERROR("cv_bridge exception: %s", e.what());
    return;
  }

  std::stringstream ss_dpt;
  ss_dpt << "dpt_" << dpt_msg->header.seq << ".pgm";
  cv::imwrite(ss_dpt.str(), cv_ptr_dpt->image);

  dpt_dataset_.push_back(std::make_pair(dpt_msg->header.stamp.toSec(), ss_dpt.str()));
}
