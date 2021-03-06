/**
 * @file trackingHough.h
 * @brief Captures the frames of the eye
 *
 * Image Processing and Pattern Recognition
 * (Bildverarbeitung und Mustererkennung)
 * Project
 */

#include "trackingHead.hpp"

#ifndef __TRACKINGHEAD_CPP__
#define __TRACKINGHEAD_CPP__

#include <iostream>

// opencv
#include "highgui.h"


//------------------------------------------------------------------------------
TrackingHead::TrackingHead(const int head_cam, int show_binary)
  : m_head_cam(head_cam), m_bw_threshold(40), m_show_binary(show_binary),
    m_hough_minDist(10), m_hough_dp(2), m_hough_param1(30), m_hough_param2(1),
    m_hough_minRadius(0), m_hough_maxRadius(20)
{
  // initialize head cam
  m_head = new HeadCapture(m_head_cam, 1);
  m_frame_height = m_head->getHeight();
  m_frame_width = m_head->getWidth();
  // define corner points (corner of head frame)
  // this is the reference the homography is mapped to
  m_corners.push_back(cv::Point2f(0, 0));
  m_corners.push_back(cv::Point2f(m_frame_width, 0));
  m_corners.push_back(cv::Point2f(m_frame_width, m_frame_height));
  m_corners.push_back(cv::Point2f(0, m_frame_height));
  // define marker points
  // these are overwritten with the actual marker points once tracked
  m_markers.push_back(cv::Point2f(0, 0));
  m_markers.push_back(cv::Point2f(m_frame_width, 0));
  m_markers.push_back(cv::Point2f(m_frame_width, m_frame_height));
  m_markers.push_back(cv::Point2f(0, m_frame_height));
}

cv::Mat TrackingHead::getFrame()
{
  m_frame = m_head->getFrame().clone();

  cv::threshold(m_frame.clone(), m_binary_frame, m_bw_threshold, 255, cv::THRESH_BINARY);

  // dilate the points because this makes it easier to find the contour and 
  // sometimes the points are so small that it is good too make them more visible
  // wouldnt be necessary if there was no IR cutoff filter in the laptop webcam
  for(int i = 0; i < 8; i++)
    cv::dilate(m_binary_frame, m_binary_frame, cv::Mat());

  // HoughCirclesMarkers is deprecated!
  //HoughCirclesMarkers();
  
  // get the ellipses around the markers
  EllipseMarkers();
  // draw the circles into the head frame
  for(int i = 0; i < m_circles.size(); i++)
    cv::circle(m_frame, cv::Point2f(m_circles[i][0], m_circles[i][1]), m_circles[0][2], cv::Scalar(255), 2);

  // get upper right corner marker, upper left corner marker and so on...
  // compare the distance of the markers to the corners in order to
  // know which marker belongs to which corner
  for(int i = 0; i < m_corners.size(); i++)
  {
    double min = std::numeric_limits<double>::max();
    for(int j = 0; j < m_circles.size(); j++)
    {
      double dist = sqrt(pow(m_corners[i].x - m_circles[j][0],2) + 
                         pow(m_corners[i].y - m_circles[j][1],2));
      if(dist < min)
      {
        min = dist;
        m_markers[i] = cv::Point2f(m_circles[j][0], m_circles[j][1]);
      }
    }
  }

  // get the homography
  //m_homography = cv::findHomography(m_markers, m_corners, 0);
  //m_homography = cv::findHomography(m_markers, m_corners, CV_RANSAC);
  m_homography = cv::getPerspectiveTransform(m_markers, m_corners);
  //m_homography = cv::getPerspectiveTransform(m_corners, m_markers);
  //m_homography = cv::findHomography(m_corners, m_markers, 0);

  // just for testing purposes
  // this warps the head frame onto the head cam dims
  //cv::Mat frame_warped;
  //cv::warpPerspective(m_frame,frame_warped,m_homography,cv::Size(m_frame_width,m_frame_height));
  //m_frame = frame_warped.clone();

  // this is mostly for testing purposes, in most cases one wants to
  // show the m_frame
  if(m_show_binary)
    return m_binary_frame.clone();
  else
    return m_frame.clone();
}
void TrackingHead::EllipseMarkers()
{
  cv::Mat gray, binary;
  // update frame (i don't think this is necessary to be honest)
  m_head->getFrame();

  binary = m_binary_frame.clone();
  gray = m_frame.clone();

  std::vector<cv::Vec3f> circles;

  // get all the contours of the markers in the binary image
  std::vector<std::vector<cv::Point> > contours;
  std::vector<cv::Vec4i> hierarchy;
  cv::findContours(m_binary_frame, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE, 
                   cv::Point(0, 0));


  // we want exactly 4 contours, everything else sucks
  if(contours.size() == 4)
  {
    for(int i = 0; i < contours.size(); i++)
    {
      // fit ellipse around each contour
      cv::RotatedRect rect_n = cv::fitEllipse(contours[i]);
      // and draw it
      cv::ellipse(m_frame, rect_n, cv::Scalar(200));
      // and push back the centers
      circles.push_back(cv::Vec3f(rect_n.center.x, rect_n.center.y, 0));
    }
    m_circles = circles;
  }
  else
  {
    // annoy the user
    if(contours.size() < 4)
      std::cout << "[HEAD TRACKING] Error: Too few markers found!" << std::endl;

    if(contours.size() > 4)
      std::cout << "[HEAD TRACKING] Error: Too many markers found!" << std::endl;
  }
}

// DEPRECATED!!!
void TrackingHead::HoughCirclesMarkers()
{
  cv::Mat gray, binary;
  //gray = m_head->getFrame();
  m_head->getFrame();

  //cv::threshold(gray, binary, m_bw_threshold, 255, cv::THRESH_BINARY);
  binary = m_binary_frame.clone();
  gray = m_frame.clone();

  //for(int i = 0; i < 18; i++)
    //cv::dilate(binary, binary, cv::Mat());

  std::vector<cv::Vec3f> circles;

  //int adapt_param2 = m_hough_param2;
  int count = 0;
  m_hough_param2++;
  while(circles.size() != 4 && m_hough_param2 > 0)
  {
    cv::HoughCircles(binary, circles, CV_HOUGH_GRADIENT, m_hough_dp, 
                     m_hough_minDist, m_hough_param1, m_hough_param2, m_hough_minRadius, 
                     m_hough_maxRadius);

    // decrease threshold if not enough circles are found
    if(circles.size() < 4)
      m_hough_param2--;
    
    if(circles.size() > 4)
      m_hough_param2++;

    if(count > 20)
      break;
    count++;
  }

  m_circles = circles;

}

//------------------------------------------------------------------------------
TrackingHead::~TrackingHead()
{
}

#endif // __TRACKINGHEAD_CPP__
