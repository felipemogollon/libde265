/*
 * H.265 video codec.
 * Copyright (c) 2013-2014 struktur AG, Dirk Farin <farin@struktur.de>
 *
 * Authors: Dirk Farin <farin@struktur.de>
 *
 * This file is part of libde265.
 *
 * libde265 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libde265 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libde265.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libde265/encoder-context.h"
#include "libde265/analyze.h"


de265_error encoder_context::encode_headers()
{
  nal_header nal;

  // VPS

  vps.set_defaults(Profile_Main, 6,2);


  // SPS

  sps.set_defaults();
  sps.set_CB_log2size_range( Log2(params.min_cb_size), Log2(params.max_cb_size));
  sps.set_TB_log2size_range( Log2(params.min_tb_size), Log2(params.max_tb_size));
  sps.max_transform_hierarchy_depth_intra = params.max_transform_hierarchy_depth_intra;

  sps.set_resolution(img_source->get_width(),
                     img_source->get_height());
  sps.compute_derived_values();

  // PPS

  pps.set_defaults();
  pps.pic_init_qp = pic_qp;

  // turn off deblocking filter
  pps.deblocking_filter_control_present_flag = true;
  pps.deblocking_filter_override_enabled_flag = false;
  pps.pic_disable_deblocking_filter_flag = true;
  pps.pps_loop_filter_across_slices_enabled_flag = false;

  pps.set_derived_values(&sps);


  // slice

  shdr.set_defaults(&pps);
  shdr.slice_deblocking_filter_disabled_flag = true;
  shdr.slice_loop_filter_across_slices_enabled_flag = false;

  img.vps  = vps;
  img.sps  = sps;
  img.pps  = pps;



  // write headers

  nal.set(NAL_UNIT_VPS_NUT);
  nal.write(cabac);
  vps.write(&errqueue, cabac);
  cabac->flush_VLC();
  write_packet();

  nal.set(NAL_UNIT_SPS_NUT);
  nal.write(cabac);
  sps.write(&errqueue, cabac);
  cabac->flush_VLC();
  write_packet();

  nal.set(NAL_UNIT_PPS_NUT);
  nal.write(cabac);
  pps.write(&errqueue, cabac, &sps);
  cabac->flush_VLC();
  write_packet();

  return DE265_OK;
}


de265_error encoder_context::encode_picture_from_input_buffer()
{
  const encoder_picture_buffer::image_data* imgdata;
  imgdata = picbuf.get_next_picture_to_encode();
  assert(imgdata);
  picbuf.mark_encoding_started(imgdata->frame_number);

  fprintf(stderr,"encoding frame %d\n",imgdata->frame_number);


  // write slice header

  //shdr.slice_pic_order_cnt_lsb = poc & 0xFF;

  nal_header nal;
  nal.set(NAL_UNIT_IDR_W_RADL);
  nal.write(cabac);
  shdr.write(&errqueue, cabac, &sps, &pps, nal.nal_unit_type);
  cabac->skip_bits(1);
  cabac->flush_VLC();

  cabac->init_CABAC();
  double psnr = encode_image(this,imgdata->input, algo);
  fprintf(stderr,"  PSNR-Y: %f\n", psnr);
  cabac->flush_CABAC();
  write_packet();


  // --- write reconstruction ---

  if (reconstruction_sink) {
    reconstruction_sink->send_image(&img);
  }


  picbuf.mark_encoding_finished(imgdata->frame_number);
}
