/*****************************************************************************

YASK: Yet Another Stencil Kit
Copyright (c) 2014-2023, Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

*****************************************************************************/

#pragma once

namespace yask {

    // Classes that support evaluation of one stencil bundle
    // and a stage of bundles.
    // A stencil solution contains one or more stages.

    // A pure-virtual class base for a stencil bundle.
    class StencilBundleBase :
        public ContextLinker {

    protected:

        // Other bundles that this one depends on.
        StencilBundleSet _depends_on;

        // List of scratch-var bundles that need to be evaluated
        // before this bundle. Listed in eval order first-to-last.
        StencilBundleList _scratch_children;

        // Overall bounding box for the bundle.
        // This may or may not be solid, i.e., it
        // may contain some invalid points.
        // This must fit inside the extended BB for this rank.
        BoundingBox _bundle_bb;

	// Bounding box(es) that indicate where this bundle is valid.
	// These must be non-overlapping. These do NOT contain
        // any invalid points. These will all be inside '_bundle_bb'.
	BBList _bb_list;

        // Max write halos for scratch bundles on left and right in each dim.
        IdxTuple max_lh, max_rh;

        // Normalize the 'orig' indices, i.e., divide by vector len in each dim.
        // Ranks offsets must already be subtracted.
        // Each dim in 'orig' must be a multiple of corresponding vec len.
        inline Indices
        normalize_indices(const Indices& orig) const {
            STATE_VARS(this);
            assert(orig.get_num_dims() == nsdims);
            Indices norm(orig);

            // i: index for stencil dims, j: index for domain dims.
            DOMAIN_VAR_LOOP_FAST(i, j) {

                // Divide indices by fold lengths as needed by
                // read/write_vec_norm().  Use idiv_flr() instead of '/'
                // because begin/end vars may be negative (e.g., if in halo).
                norm[i] = idiv_flr<idx_t>(orig[i], fold_pts[j]);

                // Check for no remainder.
                assert(imod_flr<idx_t>(orig[i], fold_pts[j]) == 0);
            }
            return norm;
        }
        inline ScanIndices
        normalize_indices(const ScanIndices& orig) {
            ScanIndices norm(orig);
            norm.begin = normalize_indices(orig.begin);
            norm.start = norm.begin;
            norm.end = normalize_indices(orig.end);
            norm.stop = norm.end;
            norm.tile_size = normalize_indices(orig.tile_size);
            norm.align = normalize_indices(orig.align);
            norm.stride = normalize_indices(orig.stride);
            return norm;
        }

    public:

        // Vars that are written to by these stencils.
        VarPtrs output_var_ptrs;

        // Vars that are read by these stencils (not necessarify
        // read-only, i.e., a var can be input and output).
        VarPtrs input_var_ptrs;

        // Vectors of scratch vars that are written to/read from.
        // Vectors contain one entry per outer thread.
        ScratchVecs output_scratch_vecs;
        ScratchVecs input_scratch_vecs;

        // ctor, dtor.
        StencilBundleBase(StencilContext* context) :
            ContextLinker(context) { }
        virtual ~StencilBundleBase() { }

        // Access to BBs.
        BoundingBox& get_bb() { return _bundle_bb; }
        BBList& get_bbs() { return _bb_list; }

        // Add dependency.
        void add_dep(StencilBundleBase* eg) {
            _depends_on.insert(eg);
        }

        // Get dependencies.
        const StencilBundleSet& get_deps() const {
            return _depends_on;
        }

        // Add needed scratch-bundle.
        void add_scratch_child(StencilBundleBase* eg) {
            _scratch_children.push_back(eg);
        }

        // Get needed scratch-bundle(s).
        const StencilBundleList& get_scratch_children() const {
            return _scratch_children;
        }

        // Get scratch children plus self.
        StencilBundleList get_reqd_bundles() {
            auto sg_list = get_scratch_children(); // Do children first.
            sg_list.push_back(this); // Do self last.
            return sg_list;
        }

        // Determine max write halos.
        void find_write_halos();

        // For scratch bundle,
        // expand indices to calculate values in scratch-halo.
        // Adjust offsets in vars based on original idxs.
        // Return adjusted indices.
        ScanIndices adjust_scratch_span(int thread_idx,
                                        const ScanIndices& idxs,
                                        KernelSettings& settings) const;

        // Set the bounding-box vars for this bundle in this rank.
        // This includes the overall-BB and the constituent full-BBs.
        void find_bounding_boxes();

        // Copy BB vars from another.
        void copy_bounding_boxes(const StencilBundleBase* src);

        // Calculate results for an arbitrary tile for points in the valid domain.
        // Scratch vars, if any, are indexed via 'scratch_var_idx'.
        virtual void
        calc_in_domain(int scratch_var_idx, const ScanIndices& misc_idxs) =0;

        // Calculate results within a micro-block.
        void
        calc_micro_block(int outer_thread_idx,
                         KernelSettings& settings,
                         const ScanIndices& micro_block_idxs,
                         MpiSection& mpisec);

        // Mark vars dirty that are updated by this bundle and/or
        // update last valid step.
        void
        update_var_info(YkVarBase::dirty_idx whose,
                        idx_t step,
                        bool mark_extern_dirty,
                        bool mod_dev_data,
                        bool update_valid_step);
        
        // Calculate results within a nano-block.
        virtual void
        calc_nano_block(int outer_thread_idx,
                       int inner_thread_idx,
                       KernelSettings& settings,
                       const ScanIndices& micro_block_idxs) =0;

        // Functions below are stubs for the code generated
        // by the stencil compiler.
        
        // Get name of this bundle.
        virtual const std::string
        get_name() const =0;

        // Get estimated number of FP ops done for one scalar eval.
        virtual int
        get_scalar_fp_ops() const =0;

        // Get number of points read and written for one scalar eval.
        virtual int
        get_scalar_points_read() const =0;
        virtual int
        get_scalar_points_written() const =0;

        // Whether this bundle updates scratch var(s)?
        virtual bool
        is_scratch() const =0;

        // Determine whether indices are in [sub-]domain.
        virtual bool
        is_in_valid_domain(const Indices& idxs) const =0;

        // Return true if there are any non-default conditions.
        virtual bool
        is_sub_domain_expr() const =0;
        virtual bool
        is_step_cond_expr() const =0;

        // Return human-readable description of conditions.
        virtual std::string
        get_domain_description() const =0;
        virtual std::string
        get_step_cond_description() const =0;

        // Determine whether step index is enabled.
        virtual bool
        is_in_valid_step(idx_t input_step_index) const =0;

        // If bundle updates var(s) with the step index,
        // set 'output_step_index' to the step that an update
        // occurs when calling one of the calc_*() methods with
        // 'input_step_index' and return 'true'.
        // Else, return 'false';
        virtual bool
        get_output_step_index(idx_t input_step_index,
                              idx_t& output_step_index) const =0;

    };                          // StencilBundleBase.

    // A template that is instantiated with the stencil-compiler
    // output class.
    template <typename StencilBundleImplT,
              typename StencilCoreDataT>
    class StencilBundleTempl:
        public StencilBundleBase {

    protected:
        StencilBundleImplT _bundle;

        // Access core data.
        // TODO: use dynamic_cast in CHECK mode.
        inline StencilCoreDataT* _corep() {
            return static_cast<StencilCoreDataT*>(_context->corep());
        }
        inline const StencilCoreDataT* _corep() const {
            return static_cast<const StencilCoreDataT*>(_context->corep());
        }
        
    public:

        // Ctor.
        StencilBundleTempl(StencilContext* context):
            StencilBundleBase(context) { }

        // Dtor.
        virtual ~StencilBundleTempl() { }

        // Get name of this bundle.
        const std::string get_name() const override {
            return _bundle._name;
        }

        // Get estimated number of FP ops done for one scalar eval.
        int get_scalar_fp_ops() const override {
            return _bundle._scalar_fp_ops;
        }

        // Get number of points read and written for one scalar eval.
        int get_scalar_points_read() const override {
            return _bundle._scalar_points_read;
        }
        int get_scalar_points_written() const override {
            return _bundle._scalar_points_written;
        }

        // Whether this bundle updates scratch var(s)?
        bool is_scratch() const override {
            return _bundle._is_scratch;
        }

        // Determine whether indices are in [sub-]domain.
        bool is_in_valid_domain(const Indices& idxs) const override {
            return _bundle.is_in_valid_domain(_corep(), idxs);
        }

        // Return true if there are any non-default conditions.
        bool is_sub_domain_expr() const override {
            return _bundle.is_sub_domain_expr();
        }
        bool is_step_cond_expr() const override {
            return _bundle.is_step_cond_expr();
        }

        // Return human-readable description of conditions.
        std::string get_domain_description() const override {
            return _bundle.get_domain_description();
        }
        std::string get_step_cond_description() const override {
            return _bundle.get_step_cond_description();
        }

        // Determine whether step index is enabled.
        bool is_in_valid_step(idx_t input_step_index) const override {
            return !_context->check_step_conds ||
                _bundle.is_in_valid_step(_corep(), input_step_index);
        }

        // If bundle updates var(s) with the step index,
        // set 'output_step_index' to the step that an update
        // occurs when calling one of the calc_*() methods with
        // 'input_step_index' and return 'true'.
        // Else, return 'false';
        bool get_output_step_index(idx_t input_step_index,
                                   idx_t& output_step_index) const override {
            return _bundle.get_output_step_index(input_step_index,
                                                 output_step_index);
        }

        // Calculate results for an arbitrary tile for points in the valid domain.
        // Scratch vars, if any are used, are indexed via 'scratch_var_idx'.
        // This is very slow and used for reference calculations.
        void
        calc_in_domain(int scratch_var_idx, const ScanIndices& misc_idxs) override {
            auto* cp = _corep();

            // Loop prefix.
            #define MISC_LOOP_INDICES misc_idxs
            #define MISC_BODY_INDICES misc_range
            #define MISC_USE_LOOP_PART_0
            #include "yask_misc_loops.hpp"

            // Loop body.  Since stride is always 1, we ignore
            // stop indices.  If point is in sub-domain for this bundle,
            // then execute the reference scalar code.  TODO: fix domain of
            // scratch vars.
            if (_bundle.is_in_valid_domain(cp, misc_range.start))
                _bundle.calc_scalar(cp, scratch_var_idx, misc_range.start);

            // Loop suffix.
            #define MISC_USE_LOOP_PART_1
            #include "yask_misc_loops.hpp"
        }
        
        // Calculate results within a nano-block.
        // Essentially just a chooser between the debug and optimized versions.
        void
        calc_nano_block(int outer_thread_idx,
                       int inner_thread_idx,
                       KernelSettings& settings,
                       const ScanIndices& micro_block_idxs) override {

            // Choose between scalar debug and optimized impls.
            if (settings.force_scalar)
                calc_nano_block_dbg(outer_thread_idx, inner_thread_idx,
                                      settings, micro_block_idxs);
            else
                calc_nano_block_opt(outer_thread_idx, inner_thread_idx,
                                   settings, micro_block_idxs);
        }

        // Calculate results for one nano-block using pure scalar code.
        // This is very slow and used for debug.
        void
        calc_nano_block_dbg(int outer_thread_idx,
                           int inner_thread_idx,
                           KernelSettings& settings,
                           const ScanIndices& micro_block_idxs) {
            STATE_VARS(this);
            TRACE_MSG("for bundle '" << get_name() << "': " <<
                      micro_block_idxs.make_range_str(false) <<
                      " via outer thread " << outer_thread_idx <<
                      " and inner thread " << inner_thread_idx);

            auto* cp = _corep();

            // Init nano-block begin & end from block start & stop indices.
            // Use the 'misc' loops. Indices for these loops will be scalar and
            // global rather than normalized as in the cluster and vector loops.
            ScanIndices sb_idxs = micro_block_idxs.create_inner();

            // Stride and alignment to 1 element.
            sb_idxs.stride.set_from_const(1);
            sb_idxs.align.set_from_const(1);
            
            calc_nano_block_dbg2(cp, outer_thread_idx, sb_idxs);
        }

        // Scalar calc loop.
        // Static to make sure offload doesn't need 'this'.
        static void
        calc_nano_block_dbg2(StencilCoreDataT* cp,
                            int outer_thread_idx,
                            const ScanIndices& misc_idxs) {

            // Scan through n-D space.
            // Set OMP loop to offload; disable OMP on host.
            #ifdef USE_OFFLOAD
            #define MISC_OMP_PRAGMA \
                _Pragma("omp target parallel for device(KernelEnv::_omp_devn) schedule(static,1)")
            #else
            #define MISC_OMP_PRAGMA
            #endif
            
            // Loop prefix.
            #define MISC_LOOP_INDICES misc_idxs
            #define MISC_BODY_INDICES misc_range
            #define MISC_USE_LOOP_PART_0
            #include "yask_misc_loops.hpp"

            // Loop body.
            // Since stride is always 1, we only need start indices.
            StencilBundleImplT::calc_scalar(cp, outer_thread_idx, misc_range.start);

            // Loop suffix.
            #define MISC_USE_LOOP_PART_1
            #include "yask_misc_loops.hpp"
        }

        // Calculate results for one nano-block.
        // The index ranges in 'micro_block_idxs' are sub-divided
        // into full vector-clusters, full vectors, and partial vectors.
        // The resulting areas are evaluated by the YASK-compiler-generated code.
        void
        calc_nano_block_opt(int outer_thread_idx,
                           int inner_thread_idx,
                           KernelSettings& settings,
                           const ScanIndices& micro_block_idxs) {
            STATE_VARS(this);
            TRACE_MSG("for bundle '" << get_name() << "': " <<
                      micro_block_idxs.make_range_str(false) <<
                      " via outer thread " << outer_thread_idx <<
                      " and inner thread " << inner_thread_idx);
            auto* cp = _corep();

            /*
              2D example:
              +--------------------+
              |                    |
              |  +--------------+  |
              |  |              |  |
              |  |   +------+   |  |
              |  |   |   <------------ full clusters (multiple vectors)
              |  |   |      |   |  |
              |  |   +------+  <------ full (unmasked, single) vectors
              |  |              |  |
              |  +--------------+ <--- partial (masked, single) vectors (peel/rem)
              |                    |
              +--------------------+

              Indices and areas in each domain dim:

              eidxs.begin
               | peel <--------- partial vecs here -------> remainder
               | |   left <------ full vecs here ----> right |
               | |    |         full clusters here       |   | eidxs.end
               | |    |                 |                |   |  |
               v v    v                 v                v   v  v
               +--+-------+---------------------------+-----+--+  "+" => compute boundaries.
                  |       |                           |     |
              +---+-------+---------------------------+-----+---+ "+" => vec-aligned boundaries.
              ^   ^       ^                            ^     ^   ^
              |   |       |                            |     |   |
              |   |      fcidxs.begin (rounded up)     |     |  ovidxs.end (rounded up)
              |  fvidxs.begin (rounded up)             |    fvidxs.end (rounded down)
             ovidxs.begin (rounded down)              fcidxs.end (rounded down)
                                                   ('end' indices are one past last)

             Also need to handle all sorts of cases where some of these
             sections are empty, the case where the peel and remainder
             overlap, and the case where the left and right full vecs
             overlap.
            */

            // Init nano-block begin & end from block start & stop indices.
            // These indices are in element units and global (NOT rank-relative).
            // All other indices below are contructed from 'sb_idxs' to ensure
            // step indices are copied properly.
            ScanIndices sb_idxs = micro_block_idxs.create_inner();

            // Strides within a nano-blk are based on pico-blk sizes.
            sb_idxs.set_strides_from_inner(settings._pico_block_sizes, 1);

            // Tiles in nano-blocks.
            sb_idxs.tile_size = settings._nano_block_tile_sizes;

            // Nano-block indices in element units and rank-relative.
            ScanIndices sb_eidxs(sb_idxs);

            // Subset of nano-block that is full clusters.
            // These indices are in element units and rank-relative.
            ScanIndices sb_fcidxs(sb_idxs);

            // Subset of nano-block that is full vectors.
            // These indices are in element units and rank-relative.
            ScanIndices sb_fvidxs(sb_idxs);

            // Superset of nano-block rounded to vector outer-boundaries as shown above.
            // These indices are in element units and rank-relative.
            ScanIndices sb_ovidxs(sb_idxs);

            // These will be set to rank-relative, so set ofs to zero.
            sb_eidxs.align_ofs.set_from_const(0);
            sb_fcidxs.align_ofs.set_from_const(0);
            sb_fvidxs.align_ofs.set_from_const(0);
            sb_ovidxs.align_ofs.set_from_const(0);

            // Flag for full clusters.
            bool do_clusters = true;
            bool do_outside_clusters = false;

            // Bit-field flags for full and partial vecs on left and right
            // in each dim.
            bit_mask_t do_left_fvecs = 0, do_right_fvecs = 0;
            bit_mask_t do_left_pvecs = 0, do_right_pvecs = 0;

            // Bit-masks for computing partial vectors in each dim.
            // Init to zeros (nothing needed).
            Indices peel_masks(idx_t(0), nddims), rem_masks(idx_t(0), nddims);

            // For each domain dim:
            // - Adjust indices to be rank-relative.
            // - Determine the subset of this nano-block that is
            //   clusters, vectors, and partial vectors.
            DOMAIN_VAR_LOOP(i, j) {

                // Rank offset.
                auto rofs = _context->rank_domain_offsets[j];

                // Begin/end of rank-relative scalar elements in this dim.
                auto ebgn = sb_idxs.begin[i] - rofs;
                auto eend = sb_idxs.end[i] - rofs;

                // Find range of full clusters.
                // These are also the inner-boundaries of the
                // full vectors.
                // NB: fcbgn will be > fcend if the nano-block
                // is within a cluster.
                auto cpts = dims->_cluster_pts[j];
                auto fcbgn = round_up_flr(ebgn, cpts);
                auto fcend = round_down_flr(eend, cpts);

                // Find range of full vectors.
                // These are also the inner-boundaries of the peel
                // and rem sections.
                // NB: fvbgn will be > fvend if the nano-block
                // is within a vector.
                auto vpts = fold_pts[j];
                auto fvbgn = round_up_flr(ebgn, vpts);
                auto fvend = round_down_flr(eend, vpts);

                // Outer vector-aligned boundaries.  Note that rounding
                // direction is opposite of full vectors, i.e., rounding
                // toward outside of nano-block. These will be used as
                // boundaries for partial vectors if needed.
                auto ovbgn = round_down_flr(ebgn, vpts);
                auto ovend = round_up_flr(eend, vpts);
                assert(ovend >= ovbgn);
                assert(ovbgn <= fvbgn);
                assert(ovend >= fvend);
                
                // Any full vectors to do on left or right?  These should
                // always be false when cluster size is 1.
                bool do_left_fvec = fvbgn < fcbgn;
                bool do_right_fvec = fvend > fcend;

                // Any partial vectors to do on left or right?
                bool do_left_pvec = ebgn < fvbgn;
                bool do_right_pvec = eend > fvend;

                // Create masks.
                idx_t pmask = 0, rmask = 0;
                if (do_left_pvec || do_right_pvec) {

                    // Calculate masks in this dim for partial vectors.
                    // 2D example: assume folding is x=4*y=4.
                    // Possible 'x' peel mask to exclude 1st 2 cols:
                    //   0 0 1 1
                    //   0 0 1 1
                    //   0 0 1 1
                    //   0 0 1 1
                    // Along 'x' edge, this mask is used to update 8 elems per vec.
                    // Possible 'y' peel mask to exclude 1st row:
                    //   0 0 0 0
                    //   1 1 1 1
                    //   1 1 1 1
                    //   1 1 1 1
                    // Along 'y' edge, this mask is used to update 12 elems per vec.
                    // In an 'x-y' corner, they are ANDed to make this mask:
                    //   0 0 0 0
                    //   0 0 1 1
                    //   0 0 1 1
                    //   0 0 1 1
                    // so that the 6 corner elements are updated per vec.

                    // Need to set upper bit.
                    idx_t mbit = idx_t(1) << (dims->_fold_pts.product() - 1);

                    // Visit points in a vec-fold to set bits for this dim's
                    // masks per the diagram above.
                    bool first_inner = dims->_fold_pts.is_first_inner();
                    dims->_fold_sizes.visit_all_points
                        (first_inner,
                         [&](const Indices& pt, size_t idx) {
                             
                             // Shift masks to next posn.
                             pmask >>= 1;
                             rmask >>= 1;

                             // If the peel point is within the nano-block,
                             // set the next bit in the mask.
                             // Index is outer begin point plus this offset.
                             idx_t pi = ovbgn + pt[j];
                             if (pi >= ebgn)
                                 pmask |= mbit;

                             // If the rem point is within the nano-block,
                             // put a 1 in the mask.
                             // Index is full-vector end point plus this offset.
                             pi = fvend + pt[j];
                             if (pi < eend)
                                 rmask |= mbit;
                             return true; // from lambda.
                         });
                    if (do_left_pvec)
                        assert(pmask != 0);
                    if (do_right_pvec)
                        assert(rmask != 0);
                }

                // Special cases: boundaries and flags that need fixing due
                // to overlaps...

                // Overlapping peel and rem, i.e., ebgn and eend are in the
                // same vector. AND peel and rem masks into one mask and do
                // peel only.
                if (do_left_pvec && do_right_pvec && ovbgn == fvend) {
                    assert(fvbgn == ovend);
                    pmask &= rmask;
                    rmask = 0;
                    do_left_pvec = true;
                    do_right_pvec = false;
                    do_left_fvec = false;
                    do_right_fvec = false;
                    do_clusters = false;
                }

                // No clusters.
                else if (fcend <= fcbgn) {

                    // Move both cluster boundaries to end
                    // of full-vec range.
                    fcbgn = fcend = fvend;
                    do_clusters = false;

                    // Any full vecs?  Do left only due to fc-range
                    // adjustment above.
                    if (do_left_fvec || do_right_fvec) {
                        do_left_fvec = true;
                        do_right_fvec = false;
                    }
                }

                // Any outside parts at all?
                if (do_left_fvec || do_right_fvec ||
                    do_left_pvec || do_right_pvec)
                    do_outside_clusters = true;

                // Save loop-local (current dim) vars.
                // ScanIndices vars.
                sb_eidxs.begin[i] = ebgn;
                sb_eidxs.end[i] = eend;
                sb_fcidxs.begin[i] = fcbgn;
                sb_fcidxs.end[i] = fcend;
                sb_fvidxs.begin[i] = fvbgn;
                sb_fvidxs.end[i] = fvend;
                sb_ovidxs.begin[i] = ovbgn;
                sb_ovidxs.end[i] = ovend;

                // Domain-dim mask vars.
                peel_masks[j] = pmask;
                rem_masks[j] = rmask;
                if (do_left_fvec)
                    set_bit(do_left_fvecs, j);
                if (do_right_fvec)
                    set_bit(do_right_fvecs, j);
                if (do_left_pvec)
                    set_bit(do_left_pvecs, j);
                if (do_right_pvec)
                    set_bit(do_right_pvecs, j);

            } // domain dims.
            TRACE_MSG("nano-blk: " << sb_idxs.make_range_str(true) <<
                      "; rank-rel: " << sb_eidxs.make_range_str(true) <<
                      "; full-clusters: " << sb_fcidxs.make_range_str(true) <<
                      "; full-vectors: " << sb_fvidxs.make_range_str(true) <<
                      "; vector bounds: " << sb_ovidxs.make_range_str(true));

            int thread_limit = actl_opts->thread_limit;
            
            // Normalized cluster indices.
            auto norm_fcidxs = normalize_indices(sb_fcidxs);

            if (!do_clusters)
                TRACE_MSG("no full clusters to calculate");

            // Full rectilinear polytope of aligned clusters: use optimized
            // code for full clusters w/o masking.
            else {
                TRACE_MSG("calculating clusters within "
                          "normalized local indices " <<
                          norm_fcidxs.make_range_str(true) <<
                          " via outer thread " << outer_thread_idx <<
                          " and inner thread " << inner_thread_idx);
                
                // Perform the calculations in this block.
                calc_clusters_opt2(cp, outer_thread_idx, inner_thread_idx,
                                   thread_limit, norm_fcidxs);
                
            } // whole clusters.

            if (!do_outside_clusters)
                TRACE_MSG("no full or partial vectors to calculate");
            else {
                TRACE_MSG("processing full and/or partial vectors "
                          "within local indices " <<
                          sb_eidxs.make_range_str(true) <<
                          " bordering full clusters at " <<
                          sb_fcidxs.make_range_str(true) <<
                          " via outer thread " << outer_thread_idx <<
                          " and inner thread " << inner_thread_idx);
                #if CPTS == 1
                THROW_YASK_EXCEPTION("(internal fault) vector border-code not expected with cluster-size==1");
                #else

                // Normalized vector indices.
                auto norm_fvidxs = normalize_indices(sb_fvidxs);
                auto norm_ovidxs = normalize_indices(sb_ovidxs);

                // Need to find range in each border part.
                // 2D example w/4 edges and 4 corners:
                // +---+------+---+
                // | lx|      |rx |
                // | ly|  ly  |ly |
                // +---+------+---+
                // |   |      |   |
                // | lx|      |rx |
                // |   |      |   |
                // +---+------+---+
                // | lx|      |rx |
                // | ry|  ry  |ry |
                // +---+------+---+
                // l=left or peel.
                // r=right or remainder.
                // Same idea for full or partial vectors, but
                // different start and stop indices.
                // Strictly, full vectors could be done with fewer parts since
                // masking isn't needed, but full vectors are only needed when
                // using clustering, and clustering is usually done at most
                // along one dim, so this optimization wouldn't help much in practice.
                int partn = 0;
                
                // Loop through progressively more intersections of domain dims, e.g.,
                // for 2D, do edges (1 dim), then corners (2-dim intersections);
                // for 3D, do faces (1 dim), then edges (2-dim intersections),
                // then corners (3-dim intersections).
                for (int k = 1; k <= nddims; k++) {

                    // Num of combos of 'k' dims, e.g.,
                    // for 2D:
                    // k=1, edges: x, y (2);
                    // k=2, corners: x-y (1);
                    // for 3D:
                    // k=1, faces: x, y, z (3);
                    // k=2, edges: x-y, x-z, y-z (3);
                    // k=3, corners: x-y-z (1),
                    auto ncombos = n_choose_k(nddims, k);

                    // Num of left-right sequences of length 'k' = 2^k, e.g.,
                    // for 2D:
                    // k=1, edges: l, r (2);
                    // k=2, corners: l-l, l-r, r-l, r-r (4)
                    // for 3D:
                    // k=1, faces: l, r (2);
                    // k=2, edges: l-l, l-r, r-l, r-r (4);
                    // k=3, corners: l-l-l, l-l-r, l-r-l, l-r-r,
                    //               r-l-l, r-l-r, r-r-l, r-r-r (8).
                    auto nseqs = bit_mask_t(1) << k;
                    
                    // Process each seq of each combo, e.g.,
                    // for 2D, 8 parts:
                    // k=1, edges: 2 seqs * 2 combos => 4 edges;
                    // k=2, corners: 4 seqs * 1 combo = 4 corners;
                    // for 3D, 26 parts:
                    // k=1, faces: 2 seqs * 3 combos => 6 faces;
                    // k=2, edges: 4 seqs * 3 combos => 12 edges;
                    // k=3, corners: 8 seqs * 1 combo => 8 corners.

                    // Each combo.
                    for (int r = 0; r < ncombos; r++) {

                        // Dims selected in this combo: 'nndims'-length
                        // bitset w/'k' bits set.
                        auto cdims = n_choose_k_set(nddims, k, r);

                        // L-R seqs: 'k'-length bitset.
                        for (bit_mask_t lr = 0; lr < nseqs; lr++) {
                            partn++;

                            // Normalized ranges for this part.  Initialize
                            // each to range for non-selected dims.  Strides
                            // are actually overridded by the STRIDE
                            // macros generated by the YASK compiler, so
                            // these settings are not needed.
                            auto fv_part(norm_fcidxs);
                            ///fv_part.stride.set_from_const(1); // 1-vector stride.
                            auto pv_part(norm_fvidxs);

                            bool fv_needed = true;
                            bool pv_needed = true;
                            bit_mask_t pv_mask = bit_mask_t(-1);

                            // Loop through each domain dim to set range for
                            // this combo and l-r seq.
                            #ifdef TRACE
                            std::string descr = std::string("part ") + std::to_string(partn) + ": '";
                            #endif
                            int nsel = 0;
                            DOMAIN_VAR_LOOP(i, j) {

                                // Is this dim selected in the current combo?
                                // If selected, is it left or right?
                                bool is_sel = is_bit_set(cdims, j);
                                if (is_sel) {
                                    bool is_left = !is_bit_set(lr, nsel);
                                    nsel++;

                                    // Set left-right ranges.
                                    // See indices diagram at beginning of this function.
                                    if (is_left) {
                                        fv_part.begin[i] = norm_fvidxs.begin[i];
                                        fv_part.end[i] = norm_fcidxs.begin[i];
                                        if (!is_bit_set(do_left_fvecs, j))
                                            fv_needed = false;
                                        pv_part.begin[i] = norm_ovidxs.begin[i];
                                        pv_part.end[i] = norm_fvidxs.begin[i];
                                        pv_mask &= peel_masks[j];
                                        if (!is_bit_set(do_left_pvecs, j))
                                            pv_needed = false;
                                    } else {
                                        fv_part.begin[i] = norm_fcidxs.end[i];
                                        fv_part.end[i] = norm_fvidxs.end[i];
                                        if (!is_bit_set(do_right_fvecs, j))
                                            fv_needed = false;
                                        pv_part.begin[i] = norm_fvidxs.end[i];
                                        pv_part.end[i] = norm_ovidxs.end[i];
                                        pv_mask &= rem_masks[j];
                                        if (!is_bit_set(do_right_pvecs, j))
                                            pv_needed = false;
                                    }
                                    #ifdef TRACE
                                    if (nsel > 1)
                                        descr += " & ";
                                    descr += std::string(is_left ? "left" : "right") + "-" +
                                        domain_dims.get_dim_name(j);
                                    #endif
                                }
                            }
                            #ifdef TRACE
                            descr += "'";
                            #endif

                            // Calc this full-vector part.
                            if (fv_needed) {
                                TRACE_MSG("calculating full vectors for " << descr <<
                                          " within normalized local indices " <<
                                          fv_part.make_range_str(true) <<
                                          " via outer thread " << outer_thread_idx <<
                                          " and inner thread " << inner_thread_idx);
                                
                                calc_vectors_opt2(cp,
                                                  outer_thread_idx, inner_thread_idx,
                                                  thread_limit, fv_part, bit_mask_t(-1));
                            }
                            //else TRACE_MSG("full vectors not needed for " << descr);

                            // Calc this partial-vector part.
                            if (pv_needed) {
                                TRACE_MSG("calculating partial vectors with mask 0x" << 
                                          std::hex << pv_mask << std::dec << " for " << descr <<
                                          " within normalized local indices " <<
                                          pv_part.make_range_str(true) <<
                                          " via outer thread " << outer_thread_idx <<
                                          " and inner thread " << inner_thread_idx);
                                
                                calc_vectors_opt2(cp,
                                                  outer_thread_idx, inner_thread_idx,
                                                  thread_limit, pv_part, pv_mask);
                            }
                            //else TRACE_MSG("partial vectors not needed for " << descr);
                            
                        } // L-R seqs.
                    } // dim combos.
                }
                #endif
            }
            
        } // calc_nano_block_opt.

        // Calculate a tile of clusters.
        // This should be the hottest function for most stencils.
        // All functions called from this one should be inlined.
        // Indices must be vec-len-normalized and rank-relative.
        // Static to make sure offload doesn't need 'this'.
        static void
        calc_clusters_opt2(StencilCoreDataT* corep,
                           int outer_thread_idx,
                           int inner_thread_idx,
                           int thread_limit,
                           ScanIndices& norm_idxs) {

            // Call code from stencil compiler.
            ssc_start();
            StencilBundleImplT::calc_clusters(corep,
                                              outer_thread_idx, inner_thread_idx,
                                              thread_limit, norm_idxs);
            ssc_stop();
        }

        // Calculate a tile of vectors using the given mask.
        // All functions called from this one should be inlined.
        // Indices must be vec-len-normalized and rank-relative.
        // Static to make sure offload doesn't need 'this'.
        static void
        calc_vectors_opt2(StencilCoreDataT* corep,
                          int outer_thread_idx,
                          int inner_thread_idx,
                          int thread_limit,
                          ScanIndices& norm_idxs,
                          bit_mask_t mask) {

            #if CPTS == 1
            THROW_YASK_EXCEPTION("(internal fault) masked-vector code not expected with cluster-size==1");
            #else
            
            // Call code from stencil compiler.
            StencilBundleImplT::calc_vectors(corep,
                                             outer_thread_idx, inner_thread_idx,
                                             thread_limit, norm_idxs, mask);
            #endif
        }

    }; // StencilBundleBase.
    
    // A collection of independent stencil bundles.
    // "Independent" implies that they may be evaluated
    // in any order.
    class Stage :
        public ContextLinker,
        public std::vector<StencilBundleBase*> {

    protected:
        std::string _name;

        // Union of bounding boxes for all non-scratch bundles in this stage.
        BoundingBox _stage_bb;

    public:

        // Perf stats for this stage.
        YaskTimer timer;
        idx_t steps_done = 0;
        Stats stats;

        // Work needed across points in this rank.
        idx_t num_reads_per_step = 0;
        idx_t num_writes_per_step = 0;
        idx_t num_fpops_per_step = 0;

        // Work done across all ranks.
        idx_t tot_reads_per_step = 0;
        idx_t tot_writes_per_step = 0;
        idx_t tot_fpops_per_step = 0;

        Stage(StencilContext* context,
              const std::string& name) :
            ContextLinker(context),
            _name(name) { }
        virtual ~Stage() { }

        const std::string& get_name() {
            return _name;
        }

        // Update the amount of work stats.
        // Print to current debug stream.
        void init_work_stats();

        // Determine whether step index is enabled.
        bool
        is_in_valid_step(idx_t input_step_index) const {
            if (!size())
                return false;

            // All step conditions must be the same, so
            // we use first non-scratch one.
            for (auto* bp : *this) {
                if (!bp->is_scratch())
                    return bp->is_in_valid_step(input_step_index);
            }
            assert("no non-scratch bundle");
            return false;
        }

        // Accessors.
        BoundingBox& get_bb() { return _stage_bb; }

        // Perf-tracking methods.
        void start_timers();
        void stop_timers();
        void add_steps(idx_t num_steps);

    }; // Stage.

} // yask namespace.
