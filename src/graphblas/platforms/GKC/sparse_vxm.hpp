/*
 * GraphBLAS Template Library (GBTL), Version 3.0
 *
 * Copyright 2020 Carnegie Mellon University, Battelle Memorial Institute, and
 * Authors.
 *
 * THIS MATERIAL WAS PREPARED AS AN ACCOUNT OF WORK SPONSORED BY AN AGENCY OF
 * THE UNITED STATES GOVERNMENT.  NEITHER THE UNITED STATES GOVERNMENT NOR THE
 * UNITED STATES DEPARTMENT OF ENERGY, NOR THE UNITED STATES DEPARTMENT OF
 * DEFENSE, NOR CARNEGIE MELLON UNIVERSITY, NOR BATTELLE, NOR ANY OF THEIR
 * EMPLOYEES, NOR ANY JURISDICTION OR ORGANIZATION THAT HAS COOPERATED IN THE
 * DEVELOPMENT OF THESE MATERIALS, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS,
 * OR USEFULNESS OR ANY INFORMATION, APPARATUS, PRODUCT, SOFTWARE, OR PROCESS
 * DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY OWNED
 * RIGHTS.
 *
 * Released under a BSD-style license, please see LICENSE file or contact
 * permission@sei.cmu.edu for full terms.
 *
 * [DISTRIBUTION STATEMENT A] This material has been approved for public release
 * and unlimited distribution.  Please see Copyright notice for non-US
 * Government use and distribution.
 *
 * DM20-0442
 */

#pragma once

#include <functional>
#include <utility>
#include <vector>
#include <iterator>
#include <iostream>
#include <bitset>
#include <graphblas/algebra.hpp>

#include "sparse_helpers.hpp"

//****************************************************************************

namespace grb
{
  namespace backend
  {
    //********************************************************************
    /// Implementation of 4.3.2 vxm: u * A
    //********************************************************************

    template <typename WVectorT,
              typename MaskT,
              typename AccumT,
              typename SemiringT,
              typename UVectorT,
              typename AMatrixT>
    inline void vxm(WVectorT &w,
                    MaskT const &mask,
                    AccumT const &accum,
                    SemiringT op,
                    UVectorT const &u,
                    AMatrixT const &A,
                    OutputControlEnum outp)
    {
      GRB_LOG_VERBOSE("w<M,z> := u +.* A");

      // =================================================================
      // Use axpy approach with the semi-ring.
      using TScalarType = typename SemiringT::result_type;
      std::vector<std::tuple<IndexType, TScalarType>> t;

      if ((A.nvals() > 0) && (u.nvals() > 0))
      {
        for (IndexType row_idx = 0; row_idx < u.size(); ++row_idx)
        {
          if (u.hasElement(row_idx) && !A[row_idx].empty())
          {
            axpy(t, op, u.extractElement(row_idx), A[row_idx]);
          }
        }
      }

      // =================================================================
      // Accumulate into Z
      using ZScalarType = typename std::conditional_t<
          std::is_same_v<AccumT, NoAccumulate>,
          TScalarType,
          decltype(accum(std::declval<typename WVectorT::ScalarType>(),
                         std::declval<TScalarType>()))>;

      std::vector<std::tuple<IndexType, ZScalarType>> z;
      ewise_or_opt_accum_1D(z, w, t, accum);

      // =================================================================
      // Copy Z into the final output, w, considering mask and replace/merge
      write_with_opt_mask_1D(w, z, mask, outp);
    }

    //**********************************************************************
    //**********************************************************************
    //**********************************************************************

    //********************************************************************
    /// Implementation of 4.3.2 vxm: u * A'
    //********************************************************************

    template <typename WVectorT,
              typename MaskT,
              typename AccumT,
              typename SemiringT,
              typename AMatrixT,
              typename UVectorT>
    inline void vxm(WVectorT &w,
                    MaskT const &mask,
                    AccumT const &accum,
                    SemiringT op,
                    UVectorT const &u,
                    TransposeView<AMatrixT> const &AT,
                    OutputControlEnum outp)
    {
      GRB_LOG_VERBOSE("w<M,z> := u +.* A'");
      auto const &A(AT.m_mat);

      // =================================================================
      // Do the basic dot-product work with the semi-ring.
      using TScalarType = typename SemiringT::result_type;
      std::vector<std::tuple<IndexType, TScalarType>> t;

      if ((A.nvals() > 0) && (u.nvals() > 0))
      {
        auto u_contents(u.getContents());
        for (IndexType row_idx = 0; row_idx < w.size(); ++row_idx)
        {
          if (!A[row_idx].empty())
          {
            TScalarType t_val;
            if (dot(t_val, u_contents, A[row_idx], op))
            {
              t.emplace_back(row_idx, t_val);
            }
          }
        }
      }

      // =================================================================
      // Accumulate into Z
      using ZScalarType = typename std::conditional_t<
          std::is_same_v<AccumT, NoAccumulate>,
          TScalarType,
          decltype(accum(std::declval<typename WVectorT::ScalarType>(),
                         std::declval<TScalarType>()))>;

      std::vector<std::tuple<IndexType, ZScalarType>> z;
      ewise_or_opt_accum_1D(z, w, t, accum);

      // =================================================================
      // Copy Z into the final output, w, considering mask and replace/merge
      write_with_opt_mask_1D(w, z, mask, outp);
    }

    template <typename VecT>
    VecT const & get_inner_mask(VectorComplementView<VecT> const & m)
    {
      return m.m_vec; 
    }
    
    template <typename VecT>
    VecT const & get_inner_mask(VectorStructureView<VecT> const & m)
    {
      return m.m_vec; 
    }

    template <typename VecT>
    VecT const & get_inner_mask(VectorStructuralComplementView<VecT> const & m)
    {
      return m.m_vec; 
    }

    template <typename VecT>
    VecT const & get_inner_mask(VecT const & m)
    {
      return m; 
    }
    
    NoMask const & get_inner_mask(NoMask const & m)
    {
      return m; 
    }

    template <typename T1, typename T2>
    constexpr bool is_basically_same_t = std::is_same<
        std::remove_const_t<std::remove_reference<T1>>,
        std::remove_const_t<std::remove_reference<T2>>>();

    template <typename T>
    using base_type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    //**********************************************************************
    /// Implementation for vxm with GKC Matrix and GKC Dense Vector: u * A
    //**********************************************************************
    // w, mask, and u are vectors. A is a matrix.
    template <typename AccumT,
              typename MaskT,
              typename SemiringT,
              typename ScalarT>
    inline void vxm(GKCDenseVector<ScalarT> &w,
                    MaskT const &mask,
                    AccumT const &accum,
                    SemiringT op,
                    GKCDenseVector<ScalarT> const &u,
                    GKCMatrix<ScalarT> const &A,
                    OutputControlEnum outp)
    {
      // Shortcut to the equivalent mxv
      //   mxv(w, mask, accum, op, grb::TransposeView(A), u, outp);
      //	 std::cout<<"DEFAULT AXPY"<<std::endl;

      //this needs to be default axpy implementation
      // std::vector<bool> accum_val(w.size(), false);
      //	  accum_val.resize(w.size());
      auto mask_vec = get_inner_mask(mask);

      constexpr bool comp = is_complement_v<MaskT> || is_structural_complement_v<MaskT>;
      constexpr bool strc = is_structure_v<MaskT> || is_structural_complement_v<MaskT>;

      if ((A.nvals() > 0) && (u.nvals() > 0))
      {

        if constexpr (std::is_same_v<AccumT, grb::NoAccumulate>)
        {
          if constexpr (std::is_same_v<base_type<decltype(mask_vec)>, grb::NoMask>)
          {
            w.clear();
          }
          else //these is a mask, but no accumulate
          {
            if (outp == REPLACE)
              w.clear();
          }
        }
        else
        {
          if constexpr (!std::is_same_v<base_type<decltype(mask_vec)>, grb::NoMask>)
          { //accumulate && mask
            using MaskTypeT = typename base_type<decltype(mask_vec)>::ScalarType;
            if (outp == REPLACE)
            {
              //remove all elements that are not in mask
              for (auto idx = 0; idx < w.size(); idx++)
              {
                bool remove;
                MaskTypeT val;
                if constexpr (!strc)
                {
                  remove = !mask_vec.boolExtractElement(idx, val);
                  remove |= !val;
                }
                else 
                {
                  remove = !mask_vec.hasElement(idx);
                }
                if constexpr (comp)
                {
                  remove = !remove;
                }
                if (remove)
                {
                    w.boolRemoveElement(idx);
                }
              }
            }
          }
        }

        using TScalarType = typename SemiringT::result_type;
        GKCDenseVector<TScalarType> t(w.size());

        #pragma omp parallel for
        for (IndexType idx = 0; idx != u.size(); ++idx)
        {
          if (u.hasElement(idx))
          {

            ScalarT uw = u[idx];

            #pragma omp parallel for
            for (auto aitr = A.idxBegin(idx), awitr = A.wgtBegin(idx);
                 aitr != A.idxEnd(idx); ++aitr, ++awitr)
            {
              if constexpr (std::is_same_v<base_type<decltype(mask_vec)>, grb::NoMask>)
              {
                if (t.hasElement(*aitr))
                {
                  ScalarT val = op.add(t[*aitr], op.mult(*awitr, uw));
                  t.setElement(*aitr, val);
                }
                else
                {
                  ScalarT val = op.mult(*awitr, uw);
                  t.setElement(*aitr, val);
                }
              }
              else
              { // masked cases
                //case: M !A R
                //case: M !A !R
                using MaskTypeT = typename base_type<decltype(mask_vec)>::ScalarType;
                MaskTypeT val;
                // if constexpr (std::is_same_v<AccumT, grb::NoAccumulate>)
                // {
                  if (comp ^ (mask_vec.boolExtractElement(*aitr, val) && (strc || val)))
                  {
                    if (t.hasElement(*aitr) ) //&&
                        // (outp == REPLACE || accum_val[*aitr]))
                    {
                      ScalarT val = op.add(t[*aitr], op.mult(*awitr, uw));
                      t.setElement(*aitr, val);
                    }
                    else
                    {
                      ScalarT val = op.mult(*awitr, uw);
                      t.setElement(*aitr, val);
                      // if (outp != REPLACE)
                      //   accum_val[*aitr] = true;
                    }
                  }
                // }
                // else
                // { //mask accum version
                //   if (comp ^ (mask_vec.boolExtractElement(*aitr, val) && (strc || val)))
                //   {
                //     if (w.hasElement(*aitr))
                //     {
                //       ScalarT val = op.add(w[*aitr], op.mult(*awitr, uw));
                //       w.setElement(*aitr, val);
                //     }
                //     else
                //     {
                //       ScalarT val = op.mult(*awitr, uw);
                //       w.setElement(*aitr, val);
                //     }
                //   }
                // }
              }
            }
          }
        } // End for loop over all AXPYs
        // Merge the result
        // (Replace/Merge, Accum/NoAccum)
        #pragma omp parallel for
        for (IndexType idx = 0; idx != w.size(); ++idx)
        {
          // TODO handle no mask!
          if constexpr (std::is_same_v<base_type<decltype(mask_vec)>, grb::NoMask>)
          {
            // Merge everything in (no mask)
            // Two cases: accumulate or no accum
            if constexpr (!std::is_same_v<AccumT, grb::NoAccumulate>)
            {
              // "If accum is NULL, z=t", hence w = z = t (with casting)
              if constexpr (std::is_same_v<TScalarType, ScalarT>)
              {
                w = std::move(t);
              }
              else
              {
                w = t;
              }
              break;
            }
            else // Need to put values in t into w
            {
              if (t.hasElement(idx))
              {
                w.mergeSetElement(idx, t[idx], accum);
              }
            }
          }
          else
          {
            using MaskTypeT = typename base_type<decltype(mask_vec)>::ScalarType;
            MaskTypeT val;
            if (comp ^ (mask_vec.boolExtractElement(idx, val) && (strc || val)))
            {
              if constexpr (!std::is_same_v<AccumT, grb::NoAccumulate>)
              {
                if (t.hasElement(idx))
                {
                  w.mergeSetElement(idx, t[idx], accum);
                  // Merge set element takes care of case where it's not already there
                } // If t has no element, nothing to do
              }
              else
              {
                if (t.hasElement(idx))
                {
                  w.setElement(idx, t[idx]);
                }
                else
                {
                  w.boolRemoveElement(idx);
                }
              }
            } // Not in mask, handle replace
            // Merge/Replace, Accum/NoAccum
            else if (outp == REPLACE)
            {
              w.boolRemoveElement(idx);
            }
          }
        }
      }
    }

    //**********************************************************************
    /// Implementation of vxm for GKC Matrix and Dense Vector: u * A'
    //**********************************************************************
    template <typename MaskT,
              typename AccumT,
              typename SemiringT,
              typename ScalarT>
    inline void vxm(GKCDenseVector<ScalarT> &w,
                    MaskT const &mask,
                    AccumT const &accum,
                    SemiringT op,
                    GKCDenseVector<ScalarT> const &u,
                    TransposeView<GKCMatrix<ScalarT>> const &AT,
                    OutputControlEnum outp)
    {
      // Shortcut to the equivalent mxv
      mxv(w, mask, accum, op, AT.m_mat, u, outp);
    }

    //**********************************************************************
    /// Implementation for mxv with GKC Matrix and GKC Sparse Vector: u * A
    // Designed for general case of masking and with a non-null accumulator.
    // w = [!m.*w]+U {[m.*w]+m.*(u*A)}
    // AXPY (Ax + y) approach
    //**********************************************************************
    template <
        typename MaskT,
        typename AccumT,
        typename SemiringT,
        typename ScalarT>
    inline void vxm(GKCSparseVector<ScalarT> &w,
                    MaskT const &mask,
                    AccumT const &accum,
                    SemiringT op,
                    GKCSparseVector<ScalarT> const &u,
                    GKCMatrix<ScalarT> const &A,
                    OutputControlEnum outp)
    {
      GRB_LOG_VERBOSE("w<M,z> := u +.* A");
      // w = [!m.*w]+U {[m.*w]+m.*(u*A)}

      // =================================================================
      // Use axpy approach with the semi-ring.
      using TScalarType = typename SemiringT::result_type;
      // Create tmp vector to place computed values
      GKCSparseVector<TScalarType> t(w.size());

      // Decision: densify mask for easy reference
      std::vector<bool> mask_vec;
      if constexpr (!std::is_same_v<MaskT, grb::NoMask>)
      {
        mask_vec = std::vector<bool>(mask.size());
        for (auto itr = mask.idxBegin(); itr < mask.idxEnd(); itr++)
        {
          mask_vec[*itr] = 1;
        }
      }

      // Accumulate is null, clear on replace due to null mask (from signature):
      if constexpr (std::is_same_v<AccumT, grb::NoAccumulate>)
      {
        if constexpr (std::is_same_v<MaskT, grb::NoMask>)
        {
          w.clear();
        }
        else // Have mask and no accum
        {
          if (outp == REPLACE)
          {
            w.clear();
          }
        }
      }
      else if constexpr (!std::is_same_v<MaskT, grb::NoMask>)
      // Have accumulate op AND a mask
      {
        if (outp == REPLACE)
        {
          // If we have a mask and the output control is REPLACE, delete
          // pre-existing elements not in the mask
          for (auto idx = 0; idx < mask_vec.size(); idx++)
          {
            if (!mask_vec[idx]) // Can reverse for complement?
            {
              w.boolRemoveElement(idx);
            }
          }
        } // Otherwise, if Merging, just leave values in place.
      }

      if ((A.nvals() > 0) && (u.nvals() > 0))
      {
        // Create flags for locking output locations
        // std::vector<char> flags(w.size(), 0);
        // for (auto &&idx : mask.getIndices())
        // {
        // flags[idx] = 1;
        // }
        auto UIst = u.idxBegin();
        auto UInd = u.idxEnd();
        auto UWst = u.wgtBegin();
        for (; UIst < UInd; UIst++, UWst++)
        {
          auto AIst = A.idxBegin(*UIst);
          auto AWst = A.wgtBegin(*UIst);
          for (; AIst < A.idxEnd(*UIst); AIst++, AWst++)
          {
            if (std::is_same_v<MaskT, grb::NoMask> || mask_vec[*AIst] != 0) // If allowed by the mask
            {
              auto res = op.mult(*UWst, *AWst);
              // CAS LOOP on flag
              // const char one(1);
              // const char two(2);
              // while (__sync_bool_compare_and_swap(flags.data()+*AIst, one, two)){};
              // Merge element with additive operation
              t.mergeSetElement(*AIst, res, grb::AdditiveMonoidFromSemiring(op));
              // Release CAS LOOP on flag
              // flags[*AIst] = one;
            }
          }
        } // End loop over input vector
        // Merge/accumulate if needed
        if constexpr (!std::is_same_v<AccumT, grb::NoAccumulate>)
        {
          auto TIst = t.idxBegin();
          auto TInd = t.idxEnd();
          auto TWst = t.wgtBegin();
          while (TIst != TInd)
          {
            w.mergeSetElement(*TIst, *TWst, accum);
            TIst++;
            TWst++;
          }
        }
        else // No accumulate, just merge or replace
        {
          if (outp == REPLACE)
          { // Set and forget
            if constexpr (std::is_same_v<ScalarT, TScalarType>)
            {
              //w.swap(t);
              w = std::move(t);
            }
            else
            {
              w = t;
            }
          }
          else // Merge into existing
          {
            auto TIst = t.idxBegin();
            auto TInd = t.idxEnd();
            auto TWst = t.wgtBegin();
            while (TIst != TInd)
            {
              w.setElement(*TIst, *TWst);
              TIst++;
              TWst++;
            }
          }
        }
      }
      w.setUnsorted();
    }

    //**********************************************************************
    /// Implementation of vxm for GKC Matrix and Dense Vector: u * A'
    //**********************************************************************
    template <typename MaskT,
              typename AccumT,
              typename SemiringT,
              typename ScalarT>
    inline void vxm(GKCSparseVector<ScalarT> &w,
                    MaskT const &mask,
                    AccumT const &accum,
                    SemiringT op,
                    GKCSparseVector<ScalarT> const &u,
                    TransposeView<GKCMatrix<ScalarT>> const &AT,
                    OutputControlEnum outp)
    {
      // Shortcut to the equivalent mxv
      mxv(w, mask, accum, op, AT.m_mat, u, outp);
    }

  } // backend
} // grb
