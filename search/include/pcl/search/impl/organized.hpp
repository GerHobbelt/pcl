/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 *
 */

#pragma once

#include <pcl/search/organized.h>
#include <pcl/common/point_tests.h> // for pcl::isFinite
#include <pcl/common/projection_matrix.h> // for getCameraMatrixFromProjectionMatrix, ...
#include <Eigen/Eigenvalues>

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> int
pcl::search::OrganizedNeighbor<PointT>::radiusSearch (const               PointT &query,
                                                      const double        radius,
                                                      Indices             &k_indices,
                                                      std::vector<float>  &k_sqr_distances,
                                                      unsigned int        max_nn) const
{
  // NAN test
  assert (isFinite (query) && "Invalid (NaN, Inf) point coordinates given to nearestKSearch!");

  // search window
  unsigned left, right, top, bottom;
  //unsigned x, y, idx;
  float squared_distance;
  const float squared_radius = radius * radius;

  k_indices.clear ();
  k_sqr_distances.clear ();

  this->getProjectedRadiusSearchBox (query, squared_radius, left, right, top, bottom);

  // iterate over search box
  if (max_nn == 0 || max_nn >= static_cast<unsigned int> (input_->size ()))
    max_nn = static_cast<unsigned int> (input_->size ());

  k_indices.reserve (max_nn);
  k_sqr_distances.reserve (max_nn);

  unsigned yEnd  = (bottom + 1) * input_->width + right + 1;
  unsigned idx  = top * input_->width + left;
  unsigned skip = input_->width - right + left - 1;
  unsigned xEnd = idx - left + right + 1;

  for (; xEnd != yEnd; idx += skip, xEnd += input_->width)
  {
    for (; idx < xEnd; ++idx)
    {
      if (!mask_[idx])
        continue;

      float dist_x = (*input_)[idx].x - query.x;
      float dist_y = (*input_)[idx].y - query.y;
      float dist_z = (*input_)[idx].z - query.z;
      squared_distance = dist_x * dist_x + dist_y * dist_y + dist_z * dist_z;
      //squared_distance = ((*input_)[idx].getVector3fMap () - query.getVector3fMap ()).squaredNorm ();
      if (squared_distance <= squared_radius)
      {
        k_indices.push_back (idx);
        k_sqr_distances.push_back (squared_distance);
        // already done ?
        if (k_indices.size () == max_nn)
        {
          if (sorted_results_)
            this->sortResults (k_indices, k_sqr_distances);
          return (max_nn);
        }
      }
    }
  }
  if (sorted_results_)
    this->sortResults (k_indices, k_sqr_distances);  
  return (static_cast<int> (k_indices.size ()));
}

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> int
pcl::search::OrganizedNeighbor<PointT>::nearestKSearch (const PointT &query,
                                                        int k,
                                                        Indices &k_indices,
                                                        std::vector<float> &k_sqr_distances) const
{
  assert (isFinite (query) && "Invalid (NaN, Inf) point coordinates given to nearestKSearch!");
  if (k < 1)
  {
    k_indices.clear ();
    k_sqr_distances.clear ();
    return (0);
  }

  Eigen::Vector3f queryvec (query.x, query.y, query.z);
  // project query point on the image plane
  //Eigen::Vector3f q = KR_ * query.getVector3fMap () + projection_matrix_.block <3, 1> (0, 3);
  Eigen::Vector3f q (KR_ * queryvec + projection_matrix_.block <3, 1> (0, 3));
  int xBegin = static_cast<int>(q [0] / q [2] + 0.5f);
  int yBegin = static_cast<int>(q [1] / q [2] + 0.5f);
  int xEnd   = xBegin + 1; // end is the pixel that is not used anymore, like in iterators
  int yEnd   = yBegin + 1;

  // the search window. This is supposed to shrink within the iterations
  unsigned left = 0;
  unsigned right = input_->width - 1;
  unsigned top = 0;
  unsigned bottom = input_->height - 1;

  std::vector <Entry> results; // sorted from smallest to largest distance
  results.reserve (k);
  // add point laying on the projection of the query point.
  if (xBegin >= 0 && 
      xBegin < static_cast<int> (input_->width) && 
      yBegin >= 0 && 
      yBegin < static_cast<int> (input_->height))
    testPoint (query, k, results, yBegin * input_->width + xBegin);
  else // point lys
  {
    // find the box that touches the image border -> don't waste time evaluating boxes that are completely outside the image!
    int dist = std::numeric_limits<int>::max ();

    if (xBegin < 0)
      dist = -xBegin;
    else if (xBegin >= static_cast<int> (input_->width))
      dist = xBegin - static_cast<int> (input_->width);

    if (yBegin < 0)
      dist = std::min (dist, -yBegin);
    else if (yBegin >= static_cast<int> (input_->height))
      dist = std::min (dist, yBegin - static_cast<int> (input_->height));

    xBegin -= dist;
    xEnd   += dist;

    yBegin -= dist;
    yEnd   += dist;
  }

  
  // stop used as isChanged as well as stop.
  bool stop = false;
  do
  {
    // increment box size
    --xBegin;
    ++xEnd;
    --yBegin;
    ++yEnd;

    // the range in x-direction which intersects with the image width
    int xFrom = xBegin;
    int xTo   = xEnd;
    clipRange (xFrom, xTo, 0, input_->width);
    
    // if x-extend is not 0
    if (xTo > xFrom)
    {
      // if upper line of the rectangle is visible and x-extend is not 0
      if (yBegin >= 0 && yBegin < static_cast<int> (input_->height))
      {
        index_t idx   = yBegin * input_->width + xFrom;
        index_t idxTo = idx + xTo - xFrom;
        for (; idx < idxTo; ++idx)
          stop = testPoint (query, k, results, idx) || stop;
      }
      

      // the row yEnd does NOT belong to the box -> last row = yEnd - 1
      // if lower line of the rectangle is visible
      if (yEnd > 0 && yEnd <= static_cast<int> (input_->height))
      {
        index_t idx   = (yEnd - 1) * input_->width + xFrom;
        index_t idxTo = idx + xTo - xFrom;

        for (; idx < idxTo; ++idx)
          stop = testPoint (query, k, results, idx) || stop;
      }
      
      // skip first row and last row (already handled above)
      int yFrom = yBegin + 1;
      int yTo   = yEnd - 1;
      clipRange (yFrom, yTo, 0, input_->height);
      
      // if we have lines in between that are also visible
      if (yFrom < yTo)
      {
        if (xBegin >= 0 && xBegin < static_cast<int> (input_->width))
        {
          index_t idx   = yFrom * input_->width + xBegin;
          index_t idxTo = yTo * input_->width + xBegin;

          for (; idx < idxTo; idx += input_->width)
            stop = testPoint (query, k, results, idx) || stop;
        }
        
        if (xEnd > 0 && xEnd <= static_cast<int> (input_->width))
        {
          index_t idx   = yFrom * input_->width + xEnd - 1;
          index_t idxTo = yTo * input_->width + xEnd - 1;

          for (; idx < idxTo; idx += input_->width)
            stop = testPoint (query, k, results, idx) || stop;
        }
        
      }
      // stop here means that the k-nearest neighbor changed -> recalculate bounding box of ellipse.
      if (stop)
        getProjectedRadiusSearchBox (query, results.back ().distance, left, right, top, bottom);
      
    }
    // now we use it as stop flag -> if bounding box is completely within the already examined search box were done!
    stop = (static_cast<int> (left)   >= xBegin && static_cast<int> (left)   < xEnd && 
            static_cast<int> (right)  >= xBegin && static_cast<int> (right)  < xEnd &&
            static_cast<int> (top)    >= yBegin && static_cast<int> (top)    < yEnd && 
            static_cast<int> (bottom) >= yBegin && static_cast<int> (bottom) < yEnd);
    
  } while (!stop);

  
  const auto results_size = results.size ();
  k_indices.resize (results_size);
  k_sqr_distances.resize (results_size);
  std::size_t idx = 0;
  for(const auto& result : results)
  {
    k_indices [idx] = result.index;
    k_sqr_distances [idx] = result.distance;
    ++idx;
  }
  
  return (static_cast<int> (results_size));
}

////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> void
pcl::search::OrganizedNeighbor<PointT>::getProjectedRadiusSearchBox (const PointT& point,
                                                                     float squared_radius,
                                                                     unsigned &minX,
                                                                     unsigned &maxX,
                                                                     unsigned &minY,
                                                                     unsigned &maxY) const
{
  Eigen::Vector3f queryvec (point.x, point.y, point.z);
  //Eigen::Vector3f q = KR_ * point.getVector3fMap () + projection_matrix_.block <3, 1> (0, 3);
  Eigen::Vector3f q (KR_ * queryvec + projection_matrix_.block <3, 1> (0, 3));

  float a = squared_radius * KR_KRT_.coeff (8) - q [2] * q [2];
  float b = squared_radius * KR_KRT_.coeff (7) - q [1] * q [2];
  float c = squared_radius * KR_KRT_.coeff (4) - q [1] * q [1];
  int min, max;
  // a and c are multiplied by two already => - 4ac -> - ac
  float det = b * b - a * c;
  if (det < 0)
  {
    minY = 0;
    maxY = input_->height - 1;
  }
  else
  {
    float y1 = static_cast<float> ((b - std::sqrt (det)) / a);
    float y2 = static_cast<float> ((b + std::sqrt (det)) / a);

    min = std::min (static_cast<int> (std::floor (y1)), static_cast<int> (std::floor (y2)));
    max = std::max (static_cast<int> (std::ceil (y1)), static_cast<int> (std::ceil (y2)));
    minY = static_cast<unsigned> (std::min (static_cast<int> (input_->height) - 1, std::max (0, min)));
    maxY = static_cast<unsigned> (std::max (std::min (static_cast<int> (input_->height) - 1, max), 0));
  }

  b = squared_radius * KR_KRT_.coeff (6) - q [0] * q [2];
  c = squared_radius * KR_KRT_.coeff (0) - q [0] * q [0];

  det = b * b - a * c;
  if (det < 0)
  {
    minX = 0;
    maxX = input_->width - 1;
  }
  else
  {
    float x1 = static_cast<float> ((b - std::sqrt (det)) / a);
    float x2 = static_cast<float> ((b + std::sqrt (det)) / a);

    min = std::min (static_cast<int> (std::floor (x1)), static_cast<int> (std::floor (x2)));
    max = std::max (static_cast<int> (std::ceil (x1)), static_cast<int> (std::ceil (x2)));
    minX = static_cast<unsigned> (std::min (static_cast<int> (input_->width)- 1, std::max (0, min)));
    maxX = static_cast<unsigned> (std::max (std::min (static_cast<int> (input_->width) - 1, max), 0));
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> void
pcl::search::OrganizedNeighbor<PointT>::computeCameraMatrix (Eigen::Matrix3f& camera_matrix) const
{
  pcl::getCameraMatrixFromProjectionMatrix (projection_matrix_, camera_matrix);
}

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> bool
pcl::search::OrganizedNeighbor<PointT>::estimateProjectionMatrix ()
{
  // internally we calculate with double but store the result into float matrices.
  projection_matrix_.setZero ();
  if (input_->height == 1 || input_->width == 1)
  {
    PCL_ERROR ("[pcl::%s::estimateProjectionMatrix] Input dataset is not organized!\n", this->getName ().c_str ());
    return false;
  }
  
  const unsigned ySkip = (std::max) (input_->height >> pyramid_level_, static_cast<unsigned>(1));
  const unsigned xSkip = (std::max) (input_->width >> pyramid_level_, static_cast<unsigned>(1));

  Indices indices;
  indices.reserve (input_->size () >> (pyramid_level_ << 1));
  
  for (unsigned yIdx = 0, idx = 0; yIdx < input_->height; yIdx += ySkip, idx += input_->width * ySkip)
  {
    for (unsigned xIdx = 0, idx2 = idx; xIdx < input_->width; xIdx += xSkip, idx2 += xSkip)
    {
      if (!mask_ [idx2])
        continue;

      indices.push_back (idx2);
    }
  }

  double residual_sqr = pcl::estimateProjectionMatrix<PointT> (input_, projection_matrix_, indices);
  PCL_DEBUG_STREAM("[pcl::" << this->getName () << "::estimateProjectionMatrix] projection matrix=" << std::endl << projection_matrix_ << std::endl << "residual_sqr=" << residual_sqr << std::endl);
  
  if (std::abs (residual_sqr) > eps_ * static_cast<float>(indices.size ()))
  {
    PCL_ERROR ("[pcl::%s::estimateProjectionMatrix] Input dataset is not from a projective device!\nResidual (MSE) %g, using %d valid points\n", this->getName ().c_str (), residual_sqr / double (indices.size()), indices.size ());
    return false;
  }

  // get left 3x3 sub matrix, which contains K * R, with K = camera matrix = [[fx s cx] [0 fy cy] [0 0 1]]
  // and R being the rotation matrix
  KR_ = projection_matrix_.topLeftCorner <3, 3> ();

  // precalculate KR * KR^T needed by calculations during nn-search
  KR_KRT_ = KR_ * KR_.transpose ();
  return true;
}

template<typename PointT> bool
pcl::search::OrganizedNeighbor<PointT>::testProjectionMatrix () const
{
  // test: project a few points at known image coordinates and test if the projected coordinates are close
  for(std::size_t i=0; i<11; ++i) {
    const std::size_t test_index = input_->size()*i/11u;
    if (!mask_[test_index])
      continue;
    const auto& test_point = (*input_)[test_index];
    pcl::PointXY q;
    if (!projectPoint(test_point, q) || std::abs(q.x-test_index%input_->width)>1 || std::abs(q.y-test_index/input_->width)>1) {
      PCL_WARN ("[pcl::%s::testProjectionMatrix] Input dataset does not seem to be from a projective device! (point %zu (%g,%g,%g) projected to pixel coordinates (%g,%g), but actual pixel coordinates are (%zu,%zu))\n",
                this->getName ().c_str (), test_index, test_point.x, test_point.y, test_point.z, q.x, q.y, static_cast<std::size_t>(test_index%input_->width), static_cast<std::size_t>(test_index/input_->width));
      return false;
    }
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> bool
pcl::search::OrganizedNeighbor<PointT>::projectPoint (const PointT& point, pcl::PointXY& q) const
{
  Eigen::Vector3f projected = KR_ * point.getVector3fMap () + projection_matrix_.block <3, 1> (0, 3);
  q.x = projected [0] / projected [2];
  q.y = projected [1] / projected [2];
  return (projected[2] != 0);
}
#define PCL_INSTANTIATE_OrganizedNeighbor(T) template class PCL_EXPORTS pcl::search::OrganizedNeighbor<T>;

