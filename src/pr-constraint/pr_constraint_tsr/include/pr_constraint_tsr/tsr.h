/* Copyright (c) 2014 Carnegie Mellon University
 * Author: Chris Dellin <cdellin@gmail.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - Neither the name of Carnegie Mellon University nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. 
 */

/* requires
 * #include <pr_constraint/holonomic.h>
 */

namespace pr_constraint_tsr
{

class TSRConstraint : public pr_constraint::HolonomicConstraint
{
public:

   /* if changed after construction, remember to call recalc()! */
   double _pose_0_w[7];
   double _Bw[6][2];
   double _pose_w_e[7];
   
   /* saved by recalc(): */
   double _pose_wprox_0[7];
   double _pose_e_wdist[7];
   int _dim;
   int _dim_volume;
   int _idim_Bw[6];
   
   TSRConstraint(double pose_0_w[7], double Bw[6][2], double pose_w_e[7]);
   virtual ~TSRConstraint();
   virtual void recalc();
   
   /* config is x y z qx qy qz qw */
   virtual int eval_dim(int * const n_config, int * const n_value);
   virtual int eval(const double * const config, double * const value);
};

} /* namespace pr_constraint_tsr */