/*****************************************************************************
 * Copyright © 2010-2014 VideoLAN
 *
 * Authors: Thomas Guillem <thomas.guillem@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "hevc_nal.h"
#include "hxxx_nal.h"

#include <vlc_common.h>
#include <vlc_bits.h>

#include <limits.h>

typedef uint8_t  nal_u1_t;
typedef uint8_t  nal_u2_t;
typedef uint8_t  nal_u3_t;
typedef uint8_t  nal_u4_t;
typedef uint8_t  nal_u5_t;
typedef uint8_t  nal_u6_t;
typedef uint8_t  nal_u7_t;
typedef uint8_t  nal_u8_t;
typedef int32_t  nal_se_t;
typedef uint32_t nal_ue_t;

typedef struct
{
    nal_u2_t profile_space;
    nal_u1_t tier_flag;
    nal_u5_t profile_idc;
    uint32_t profile_compatibility_flag; /* nal_u1_t * 32 */
    nal_u1_t progressive_source_flag;
    nal_u1_t interlaced_source_flag;
    nal_u1_t non_packed_constraint_flag;
    nal_u1_t frame_only_constraint_flag;
    struct
    {
        nal_u1_t max_12bit_constraint_flag;
        nal_u1_t max_10bit_constraint_flag;
        nal_u1_t max_8bit_constraint_flag;
        nal_u1_t max_422chroma_constraint_flag;
        nal_u1_t max_420chroma_constraint_flag;
        nal_u1_t max_monochrome_constraint_flag;
        nal_u1_t intra_constraint_flag;
        nal_u1_t one_picture_only_constraint_flag;
        nal_u1_t lower_bit_rate_constraint_flag;
    } idc4to7;
    struct
    {
        nal_u1_t inbld_flag;
    } idc1to5;
} hevc_inner_profile_tier_level_t;

#define HEVC_MAX_SUBLAYERS 8
typedef struct
{
    hevc_inner_profile_tier_level_t general;
    nal_u8_t general_level_idc;
    uint8_t  sublayer_profile_present_flag;  /* nal_u1_t * 8 */
    uint8_t  sublayer_level_present_flag;    /* nal_u1_t * 8 */
    hevc_inner_profile_tier_level_t sub_layer[HEVC_MAX_SUBLAYERS];
    nal_u8_t sub_layer_level_idc[HEVC_MAX_SUBLAYERS];
} hevc_profile_tier_level_t;

struct hevc_sequence_parameter_set_t
{
    nal_u4_t sps_video_parameter_set_id;
    nal_u3_t sps_max_sub_layers_minus1;
    nal_u1_t sps_temporal_id_nesting_flag;

    hevc_profile_tier_level_t profile_tier_level;

    nal_ue_t sps_seq_parameter_set_id;
    nal_ue_t chroma_format_idc;
    nal_u1_t separate_colour_plane_flag;

    nal_ue_t pic_width_in_luma_samples;
    nal_ue_t pic_height_in_luma_samples;

    nal_u1_t conformance_window_flag;
    struct
    {
    nal_ue_t left_offset;
    nal_ue_t right_offset;
    nal_ue_t top_offset;
    nal_ue_t bottom_offset;
    } conf_win;

    nal_ue_t bit_depth_luma_minus8;
    nal_ue_t bit_depth_chroma_minus8;
    nal_ue_t log2_max_pic_order_cnt_lsb_minus4;

    nal_u1_t sps_sub_layer_ordering_info_present_flag;
    struct
    {
    nal_ue_t dec_pic_buffering_minus1;
    nal_ue_t num_reorder_pics;
    nal_ue_t latency_increase_plus1;
    } sps_max[1 + HEVC_MAX_SUBLAYERS];

    nal_ue_t log2_min_luma_coding_block_size_minus3;
    nal_ue_t log2_diff_max_min_luma_coding_block_size;
    nal_ue_t log2_min_luma_transform_block_size_minus2;
    nal_ue_t log2_diff_max_min_luma_transform_block_size;

    /* incomplete */
};

struct hevc_picture_parameter_set_t
{
    nal_ue_t pps_pic_parameter_set_id;
    nal_ue_t pps_seq_parameter_set_id;
    nal_u1_t dependent_slice_segments_enabled_flag;
    nal_u1_t output_flag_present_flag;
    nal_u3_t num_extra_slice_header_bits;
    nal_u1_t sign_data_hiding_enabled_flag;
    nal_u1_t cabac_init_present_flag;
    nal_ue_t num_ref_idx_l0_default_active_minus1;
    nal_ue_t num_ref_idx_l1_default_active_minus1;
    nal_se_t init_qp_minus26;
    nal_u1_t constrained_intra_pred_flag;
    nal_u1_t transform_skip_enabled_flag;

    nal_u1_t cu_qp_delta_enabled_flag;
    nal_ue_t diff_cu_qp_delta_depth;

    nal_se_t pps_cb_qp_offset;
    nal_se_t pps_cr_qp_offset;
    nal_u1_t pic_slice_level_chroma_qp_offsets_present_flag;
    nal_u1_t weighted_pred_flag;
    nal_u1_t weighted_bipred_flag;
    nal_u1_t transquant_bypass_enable_flag;

    nal_u1_t tiles_enabled_flag;
    nal_u1_t entropy_coding_sync_enabled_flag;
    nal_ue_t num_tile_columns_minus1;
    nal_ue_t num_tile_rows_minus1;
    nal_u1_t uniform_spacing_flag;
    // nal_ue_t *p_column_width_minus1; read but discarded
    // nal_ue_t *p_row_height_minus1; read but discarded
    nal_u1_t loop_filter_across_tiles_enabled_flag;

    nal_u1_t pps_loop_filter_across_slices_enabled_flag;

    nal_u1_t deblocking_filter_control_present_flag;
    nal_u1_t deblocking_filter_override_enabled_flag;
    nal_u1_t pps_deblocking_filter_disabled_flag;
    nal_se_t pps_beta_offset_div2;
    nal_se_t pps_tc_offset_div2;

    nal_u1_t scaling_list_data_present_flag;
    // scaling_list_data; read but discarded

    nal_u1_t lists_modification_present_flag;
    nal_ue_t log2_parallel_merge_level_minus2;
    nal_u1_t slice_header_extension_present_flag;

    nal_u1_t pps_extension_present_flag;
    nal_u1_t pps_range_extension_flag;
    nal_u1_t pps_multilayer_extension_flag;
    nal_u1_t pps_3d_extension_flag;
    nal_u5_t pps_extension_5bits;
    /* incomplete */

};

/* Computes size and does check the whole struct integrity */
static size_t get_hvcC_to_AnnexB_NAL_size( const uint8_t *p_buf, size_t i_buf )
{
    size_t i_total = 0;

    if( i_buf < HEVC_MIN_HVCC_SIZE )
        return 0;

    const uint8_t i_nal_length_size = (p_buf[21] & 0x03) + 1;
    if(i_nal_length_size == 3)
        return 0;

    const uint8_t i_num_array = p_buf[22];
    p_buf += 23; i_buf -= 23;

    for( uint8_t i = 0; i < i_num_array; i++ )
    {
        if(i_buf < 3)
            return 0;

        const uint16_t i_num_nalu = p_buf[1] << 8 | p_buf[2];
        p_buf += 3; i_buf -= 3;

        for( uint16_t j = 0; j < i_num_nalu; j++ )
        {
            if(i_buf < 2)
                return 0;

            const uint16_t i_nalu_length = p_buf[0] << 8 | p_buf[1];
            if(i_buf < i_nalu_length)
                return 0;

            i_total += i_nalu_length + i_nal_length_size;
            p_buf += i_nalu_length + 2;
            i_buf -= i_nalu_length + 2;
        }
    }

    return i_total;
}

uint8_t * hevc_hvcC_to_AnnexB_NAL( const uint8_t *p_buf, size_t i_buf,
                                   size_t *pi_result, uint8_t *pi_nal_length_size )
{
    *pi_result = get_hvcC_to_AnnexB_NAL_size( p_buf, i_buf ); /* Does all checks */
    if( *pi_result == 0 )
        return NULL;

    if( pi_nal_length_size )
        *pi_nal_length_size = (p_buf[21] & 0x03) + 1;

    uint8_t *p_ret;
    uint8_t *p_out_buf = p_ret = malloc( *pi_result );
    if( !p_out_buf )
    {
        *pi_result = 0;
        return NULL;
    }

    const uint8_t i_num_array = p_buf[22];
    p_buf += 23;

    for( uint8_t i = 0; i < i_num_array; i++ )
    {
        const uint16_t i_num_nalu = p_buf[1] << 8 | p_buf[2];
        p_buf += 3;

        for( uint16_t j = 0; j < i_num_nalu; j++ )
        {
            const uint16_t i_nalu_length = p_buf[0] << 8 | p_buf[1];

            memcpy( p_out_buf, annexb_startcode4, 4 );
            memcpy( &p_out_buf[4], &p_buf[2], i_nalu_length );

            p_out_buf += 4 + i_nalu_length;
            p_buf += 2 + i_nalu_length;
        }
    }

    return p_ret;
}

static bool hevc_parse_scaling_list_rbsp( bs_t *p_bs )
{
    if( bs_remain( p_bs ) < 16 )
        return false;

    for( int i=0; i<4; i++ )
    {
        for( int j=0; j<6; j += (i == 3) ? 3 : 1 )
        {
            if( bs_read1( p_bs ) == 0 )
                bs_read_ue( p_bs );
            else
            {
                unsigned nextCoef = 8;
                unsigned coefNum = __MIN( 64, (1 << (4 + (i << 1))));
                if( i > 1 )
                {
                    nextCoef = bs_read_se( p_bs ) + 8;
                }
                for( unsigned k=0; k<coefNum; k++ )
                {
                    nextCoef = ( nextCoef + bs_read_se( p_bs ) + 256 ) % 256;
                }
            }
        }
    }

    return true;
}

static bool hevc_parse_inner_profile_tier_level_rbsp( bs_t *p_bs,
                                                      hevc_inner_profile_tier_level_t *p_in )
{
    if( bs_remain( p_bs ) < 88 )
        return false;

    p_in->profile_space = bs_read( p_bs, 2 );
    p_in->tier_flag = bs_read1( p_bs );
    p_in->profile_idc = bs_read( p_bs, 5 );
    p_in->profile_compatibility_flag = bs_read( p_bs, 32 );
    p_in->progressive_source_flag = bs_read1( p_bs );
    p_in->interlaced_source_flag = bs_read1( p_bs );
    p_in->non_packed_constraint_flag = bs_read1( p_bs );
    p_in->frame_only_constraint_flag = bs_read1( p_bs );

    if( ( p_in->profile_idc >= 4 && p_in->profile_idc <= 7 ) ||
        ( p_in->profile_compatibility_flag & 0x0F000000 ) )
    {
        p_in->idc4to7.max_12bit_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_10bit_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_8bit_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_422chroma_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_420chroma_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.max_monochrome_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.intra_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.one_picture_only_constraint_flag = bs_read1( p_bs );
        p_in->idc4to7.lower_bit_rate_constraint_flag = bs_read1( p_bs );
        (void) bs_read( p_bs, 2 );
    }
    else
    {
        (void) bs_read( p_bs, 11 );
    }
    (void) bs_read( p_bs, 32 );

    if( ( p_in->profile_idc >= 1 && p_in->profile_idc <= 5 ) ||
        ( p_in->profile_compatibility_flag & 0x7C000000 ) )
        p_in->idc1to5.inbld_flag = bs_read1( p_bs );
    else
        (void) bs_read1( p_bs );

    return true;
}

static bool hevc_parse_profile_tier_level_rbsp( bs_t *p_bs, bool profile_present,
                                                uint8_t max_num_sub_layers_minus1,
                                                hevc_profile_tier_level_t *p_ptl )
{
    if( profile_present && !hevc_parse_inner_profile_tier_level_rbsp( p_bs, &p_ptl->general ) )
        return false;

    if( bs_remain( p_bs ) < 8)
        return false;

    p_ptl->general_level_idc = bs_read( p_bs, 8 );

    if( max_num_sub_layers_minus1 > 0 )
    {
        if( bs_remain( p_bs ) < 16 )
            return false;

        for( uint8_t i=0; i< 8; i++ )
        {
            if( i < max_num_sub_layers_minus1 )
            {
                if( bs_read1( p_bs ) )
                    p_ptl->sublayer_profile_present_flag |= (0x80 >> i);
                if( bs_read1( p_bs ) )
                    p_ptl->sublayer_level_present_flag |= (0x80 >> i);
            }
            else
                bs_read( p_bs, 2 );
        }

        for( uint8_t i=0; i < max_num_sub_layers_minus1; i++ )
        {
            if( ( p_ptl->sublayer_profile_present_flag & (0x80 >> i) ) &&
                ! hevc_parse_inner_profile_tier_level_rbsp( p_bs, &p_ptl->sub_layer[i] ) )
                return false;

            if( p_ptl->sublayer_profile_present_flag & (0x80 >> i) )
            {
                if( bs_remain( p_bs ) < 8 )
                    return false;
                p_ptl->sub_layer_level_idc[i] = bs_read( p_bs, 8 );
            }
        }
    }

    return true;
}

static bool hevc_parse_sequence_parameter_set_rbsp( bs_t *p_bs,
                                                    hevc_sequence_parameter_set_t *p_sps )
{
    p_sps->sps_video_parameter_set_id = bs_read( p_bs, 4 );
    p_sps->sps_max_sub_layers_minus1 = bs_read( p_bs, 3 );
    p_sps->sps_temporal_id_nesting_flag = bs_read1( p_bs );
    if( !hevc_parse_profile_tier_level_rbsp( p_bs, true, p_sps->sps_max_sub_layers_minus1,
                                            &p_sps->profile_tier_level ) )
        return false;

    if( bs_remain( p_bs ) < 1 )
        return false;

    p_sps->sps_seq_parameter_set_id = bs_read_ue( p_bs );
    if( p_sps->sps_seq_parameter_set_id >= HEVC_SPS_MAX )
        return false;

    p_sps->chroma_format_idc = bs_read_ue( p_bs );
    if( p_sps->chroma_format_idc == 3 )
        p_sps->separate_colour_plane_flag = bs_read1( p_bs );
    p_sps->pic_width_in_luma_samples = bs_read_ue( p_bs );
    p_sps->pic_height_in_luma_samples = bs_read_ue( p_bs );
    if( !p_sps->pic_width_in_luma_samples || !p_sps->pic_height_in_luma_samples )
        return false;

    p_sps->conformance_window_flag = bs_read1( p_bs );
    if( p_sps->conformance_window_flag )
    {
        p_sps->conf_win.left_offset = bs_read_ue( p_bs );
        p_sps->conf_win.right_offset = bs_read_ue( p_bs );
        p_sps->conf_win.top_offset = bs_read_ue( p_bs );
        p_sps->conf_win.bottom_offset = bs_read_ue( p_bs );
    }

    p_sps->bit_depth_luma_minus8 = bs_read_ue( p_bs );
    p_sps->bit_depth_chroma_minus8 = bs_read_ue( p_bs );
    p_sps->log2_max_pic_order_cnt_lsb_minus4 = bs_read_ue( p_bs );

    p_sps->sps_sub_layer_ordering_info_present_flag = bs_read1( p_bs );
    for( uint8_t i=(p_sps->sps_sub_layer_ordering_info_present_flag ? 0 : p_sps->sps_max_sub_layers_minus1);
         i <= p_sps->sps_max_sub_layers_minus1; i++ )
    {
        p_sps->sps_max[i].dec_pic_buffering_minus1 = bs_read_ue( p_bs );
        p_sps->sps_max[i].num_reorder_pics = bs_read_ue( p_bs );
        p_sps->sps_max[i].latency_increase_plus1 = bs_read_ue( p_bs );
    }

    if( bs_remain( p_bs ) < 4 )
        return false;

    p_sps->log2_min_luma_coding_block_size_minus3 = bs_read_ue( p_bs );
    p_sps->log2_diff_max_min_luma_coding_block_size = bs_read_ue( p_bs );
    p_sps->log2_min_luma_transform_block_size_minus2 = bs_read_ue( p_bs );
    if( bs_remain( p_bs ) < 1 ) /* last late fail check */
        return false;
    p_sps->log2_diff_max_min_luma_transform_block_size = bs_read_ue( p_bs );

    /* parsing incomplete */
    return true;
}

void hevc_rbsp_release_sps( hevc_sequence_parameter_set_t *p_sps )
{
    free( p_sps );
}

hevc_sequence_parameter_set_t * hevc_rbsp_decode_sps( const uint8_t *p_buf, size_t i_buf )
{
    hevc_sequence_parameter_set_t *p_sps = calloc(1, sizeof(hevc_sequence_parameter_set_t));
    if(likely(p_sps))
    {
        bs_t bs;
        bs_init( &bs, p_buf, i_buf );
        bs_skip( &bs, 16 ); /* Skip nal_unit_header */
        if( !hevc_parse_sequence_parameter_set_rbsp( &bs, p_sps ) )
        {
            hevc_rbsp_release_sps( p_sps );
            p_sps = NULL;
        }
    }
    return p_sps;
}

static bool hevc_parse_pic_parameter_set_rbsp( bs_t *p_bs,
                                               hevc_picture_parameter_set_t *p_pps )
{
    if( bs_remain( p_bs ) < 1 )
        return false;
    p_pps->pps_pic_parameter_set_id = bs_read_ue( p_bs );
    if( p_pps->pps_pic_parameter_set_id >= HEVC_PPS_MAX || bs_remain( p_bs ) < 1 )
        return false;
    p_pps->pps_seq_parameter_set_id = bs_read_ue( p_bs );
    if( p_pps->pps_seq_parameter_set_id >= HEVC_SPS_MAX )
        return false;
    p_pps->dependent_slice_segments_enabled_flag = bs_read1( p_bs );
    p_pps->output_flag_present_flag = bs_read1( p_bs );
    p_pps->num_extra_slice_header_bits = bs_read( p_bs, 3 );
    p_pps->sign_data_hiding_enabled_flag = bs_read1( p_bs );
    p_pps->cabac_init_present_flag = bs_read1( p_bs );

    p_pps->num_ref_idx_l0_default_active_minus1 = bs_read_ue( p_bs );
    p_pps->num_ref_idx_l1_default_active_minus1 = bs_read_ue( p_bs );

    p_pps->init_qp_minus26 = bs_read_se( p_bs );
    p_pps->constrained_intra_pred_flag = bs_read1( p_bs );
    p_pps->transform_skip_enabled_flag = bs_read1( p_bs );
    p_pps->cu_qp_delta_enabled_flag = bs_read1( p_bs );
    if( p_pps->cu_qp_delta_enabled_flag )
        p_pps->diff_cu_qp_delta_depth = bs_read_ue( p_bs );

    if( bs_remain( p_bs ) < 1 )
        return false;

    p_pps->pps_cb_qp_offset = bs_read_se( p_bs );
    p_pps->pps_cr_qp_offset = bs_read_se( p_bs );
    p_pps->pic_slice_level_chroma_qp_offsets_present_flag = bs_read1( p_bs );
    p_pps->weighted_pred_flag = bs_read1( p_bs );
    p_pps->weighted_bipred_flag = bs_read1( p_bs );
    p_pps->transquant_bypass_enable_flag = bs_read1( p_bs );
    p_pps->tiles_enabled_flag = bs_read1( p_bs );
    p_pps->entropy_coding_sync_enabled_flag = bs_read1( p_bs );

    if( p_pps->tiles_enabled_flag )
    {
        p_pps->num_tile_columns_minus1 = bs_read_ue( p_bs ); /* TODO: validate max col/row values */
        p_pps->num_tile_rows_minus1 = bs_read_ue( p_bs );    /*       against sps PicWidthInCtbsY */
        p_pps->uniform_spacing_flag = bs_read1( p_bs );
        if( !p_pps->uniform_spacing_flag )
        {
            for( unsigned i=0; i< p_pps->num_tile_columns_minus1; i++ )
                (void) bs_read_ue( p_bs );
            for( unsigned i=0; i< p_pps->num_tile_rows_minus1; i++ )
                (void) bs_read_ue( p_bs );
        }
        p_pps->loop_filter_across_tiles_enabled_flag = bs_read1( p_bs );
    }

    p_pps->pps_loop_filter_across_slices_enabled_flag = bs_read1( p_bs );
    p_pps->deblocking_filter_control_present_flag = bs_read1( p_bs );
    if( p_pps->deblocking_filter_control_present_flag )
    {
        p_pps->deblocking_filter_override_enabled_flag = bs_read1( p_bs );
        p_pps->pps_deblocking_filter_disabled_flag = bs_read1( p_bs );
        if( !p_pps->pps_deblocking_filter_disabled_flag )
        {
            p_pps->pps_beta_offset_div2 = bs_read_se( p_bs );
            p_pps->pps_tc_offset_div2 = bs_read_se( p_bs );
        }
    }

    p_pps->scaling_list_data_present_flag = bs_read1( p_bs );
    if( p_pps->scaling_list_data_present_flag && !hevc_parse_scaling_list_rbsp( p_bs ) )
        return false;

    p_pps->lists_modification_present_flag = bs_read1( p_bs );
    p_pps->log2_parallel_merge_level_minus2 = bs_read_ue( p_bs );
    p_pps->slice_header_extension_present_flag = bs_read1( p_bs );

    if( bs_remain( p_bs ) < 1 )
        return false;

    p_pps->pps_extension_present_flag = bs_read1( p_bs );
    if( p_pps->pps_extension_present_flag )
    {
        p_pps->pps_range_extension_flag = bs_read1( p_bs );
        p_pps->pps_multilayer_extension_flag = bs_read1( p_bs );
        p_pps->pps_3d_extension_flag = bs_read1( p_bs );
        if( bs_remain( p_bs ) < 5 )
            return false;
        p_pps->pps_extension_5bits = bs_read( p_bs, 5 );
    }

    return true;
}

void hevc_rbsp_release_pps( hevc_picture_parameter_set_t *p_pps )
{
    free( p_pps );
}

hevc_picture_parameter_set_t * hevc_rbsp_decode_pps( const uint8_t *p_buf, size_t i_buf )
{
    hevc_picture_parameter_set_t *p_pps = calloc(1, sizeof(hevc_picture_parameter_set_t));
    if(likely(p_pps))
    {
        bs_t bs;
        bs_init( &bs, p_buf, i_buf );
        bs_skip( &bs, 16 ); /* Skip nal_unit_header */
        if( !hevc_parse_pic_parameter_set_rbsp( &bs, p_pps ) )
        {
            hevc_rbsp_release_pps( p_pps );
            p_pps = NULL;
        }
    }
    return p_pps;
}
