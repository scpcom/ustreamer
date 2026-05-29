#include <condition_variable>
#include <map>
#include <mutex>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif
#include <ax_sys_api.h>
#include <ax_engine_api.h>
#ifdef __cplusplus
}
#endif

#include "ax_middleware.hpp"
#include "maix_nn_maixcam.hpp"



namespace maix::nn
{

uint8_t dtype_ax2maix(uint eDataType)

{
  return 0;
}



// NN_MaixCam::loaded()

bool NN_MaixCam::loaded()

{
  return false;
}



// NN_MaixCam::set_dual_buff(bool)

void NN_MaixCam::set_dual_buff(bool enable)

{
  return;
}



// free_io_index(_AX_ENGINE_IO_BUFFER_T*, unsigned long)

void free_io_index(_AX_ENGINE_IO_BUFFER_T *pEngineIoBuf,ulong index)

{
  return;
}



// free_io(_AX_ENGINE_IO_T*)

void free_io(_AX_ENGINE_IO_T *pEngineIo)

{
  return;
}



// prepare_io(_AX_ENGINE_IO_INFO_T*, _AX_ENGINE_IO_T*, bool, bool)

int prepare_io
              (_AX_ENGINE_IO_INFO_T *pIoInfo,_AX_ENGINE_IO_T *pIo,bool bInCached,bool bOutCached)

{
  return -1;
}



// _print_axmodel_shape(_AX_ENGINE_IOMETA_T*)

void _print_axmodel_shape(_AX_ENGINE_IOMETA_T *pEngineIoMeta)

{
  return;
}



// NN_MaixCam::unload()

err::Err NN_MaixCam::unload()

{
  return err::ERR_NOT_IMPL;
}



// NN_MaixCam::~NN_MaixCam()

NN_MaixCam::~NN_MaixCam()

{
  return;
}



// NN_MaixCam::_init(bool)

void NN_MaixCam::_init(bool dual_buff)

{
  return;
}



// NN_MaixCam::NN_MaixCam()

NN_MaixCam::NN_MaixCam()

{
  return;
}



// NN_MaixCam::NN_MaixCam(bool)

NN_MaixCam::NN_MaixCam(bool dual_buff)

{
  return;
}



// mud_load_raw_model(std::__cxx11::string const&, MUD*)

err::Err mud_load_raw_model(const std::string &model_path, MUD *mud_obj)

{
  return err::ERR_NOT_IMPL;
}



// NN_MaixCam::load(MUD const&, std::__cxx11::string const&)

err::Err NN_MaixCam::load(const MUD &mud, const std::string &dir)

{
  return err::ERR_NOT_IMPL;
}



// _print_tensor_shape(maix::tensor::Tensor*)

void _print_tensor_shape(tensor::Tensor *tensor)

{
  return;
}



// NN_MaixCam::forward(maix::tensor::Tensors&, maix::tensor::Tensors&, bool, bool)

err::Err NN_MaixCam::forward(tensor::Tensors &inputs, tensor::Tensors &outputs, bool copy_result, bool dual_buff_wait)

{
  return err::ERR_NOT_IMPL;
}



// NN_MaixCam::forward(maix::tensor::Tensors&, bool, bool)

tensor::Tensors *NN_MaixCam::forward(tensor::Tensors &inputs, bool copy_result, bool dual_buff_wait)

{
  return NULL;
}



// NN_MaixCam::forward_image(maix::image::Image&, std::vector<float, std::allocator<float>
// >, std::vector<float, std::allocator<float> >, maix::image::Fit, bool, bool, bool)

tensor::Tensors *NN_MaixCam::forward_image(image::Image &img, std::vector<float> mean, std::vector<float> scale, image::Fit fit, bool copy_result, bool dual_buff_wait, bool chw)

{
  return NULL;
}



// NN_MaixCam::inputs_info()

std::vector<LayerInfo> NN_MaixCam::inputs_info()

{
  return std::vector<LayerInfo>();
}



// NN_MaixCam::outputs_info()

std::vector<LayerInfo> NN_MaixCam::outputs_info()

{
  return std::vector<LayerInfo>();
}



// _get_sample_class_distance(std::vector<int, std::allocator<int> >&, std::vector<float*,
// std::allocator<float*> >&, std::vector<float*, std::allocator<float*> >&, int)

float _get_sample_class_distance
                (std::vector<int> &param_1,std::vector<float *> &features,std::vector<float *> &features_samples,int feature_num)

{
  return 0.0;
}



/* maix::nn::maix_nn_self_learn_classifier_learn(std::vector<float*, std::allocator<float*> >&,
   std::vector<float*, std::allocator<float*> >&, int) */

int maix_nn_self_learn_classifier_learn(std::vector<float *> &features, std::vector<float *> &features_samples, int feature_num)

{
  return 0;
}



// namespace
}
