// sherpa-onnx/csrc/offline-recognizer-impl.cc
//
// Copyright (c)  2023  Xiaomi Corporation

#include "sherpa-onnx/csrc/offline-recognizer-impl.h"

#include <string>

#include "onnxruntime_cxx_api.h"  // NOLINT
#include "sherpa-onnx/csrc/macros.h"
#include "sherpa-onnx/csrc/offline-recognizer-ctc-impl.h"
#include "sherpa-onnx/csrc/offline-recognizer-paraformer-impl.h"
#include "sherpa-onnx/csrc/offline-recognizer-transducer-impl.h"
#include "sherpa-onnx/csrc/onnx-utils.h"
#include "sherpa-onnx/csrc/text-utils.h"

namespace sherpa_onnx {

std::unique_ptr<OfflineRecognizerImpl> OfflineRecognizerImpl::Create(
    const OfflineRecognizerConfig &config) {
  Ort::Env env(ORT_LOGGING_LEVEL_ERROR);

  Ort::SessionOptions sess_opts;
  std::string model_filename;
  if (!config.model_config.transducer.encoder_filename.empty()) {
    model_filename = config.model_config.transducer.encoder_filename;
  } else if (!config.model_config.paraformer.model.empty()) {
    model_filename = config.model_config.paraformer.model;
  } else if (!config.model_config.nemo_ctc.model.empty()) {
    model_filename = config.model_config.nemo_ctc.model;
  } else {
    SHERPA_ONNX_LOGE("Please provide a model");
    exit(-1);
  }

  auto buf = ReadFile(model_filename);

  auto encoder_sess =
      std::make_unique<Ort::Session>(env, buf.data(), buf.size(), sess_opts);

  Ort::ModelMetadata meta_data = encoder_sess->GetModelMetadata();

  Ort::AllocatorWithDefaultOptions allocator;  // used in the macro below

  auto model_type_ptr =
      meta_data.LookupCustomMetadataMapAllocated("model_type", allocator);
  if (!model_type_ptr) {
    SHERPA_ONNX_LOGE(
        "No model_type in the metadata!\n\n"
        "Please refer to the following URLs to add metadata"
        "\n"
        "(0) Transducer models from icefall"
        "\n    "
        "https://github.com/k2-fsa/icefall/blob/master/egs/librispeech/ASR/"
        "pruned_transducer_stateless7/export-onnx.py#L303"
        "\n"
        "(1) Nemo CTC models\n    "
        "https://huggingface.co/csukuangfj/"
        "sherpa-onnx-nemo-ctc-en-citrinet-512/blob/main/add-model-metadata.py"
        "\n"
        "(2) Paraformer"
        "\n    "
        "https://huggingface.co/csukuangfj/"
        "paraformer-onnxruntime-python-example/blob/main/add-model-metadata.py"
        "\n");
    exit(-1);
  }
  std::string model_type(model_type_ptr.get());

  if (model_type == "conformer" || model_type == "zipformer") {
    return std::make_unique<OfflineRecognizerTransducerImpl>(config);
  }

  if (model_type == "paraformer") {
    return std::make_unique<OfflineRecognizerParaformerImpl>(config);
  }

  if (model_type == "EncDecCTCModelBPE") {
    return std::make_unique<OfflineRecognizerCtcImpl>(config);
  }

  SHERPA_ONNX_LOGE(
      "\nUnsupported model_type: %s\n"
      "We support only the following model types at present: \n"
      " - Non-streaming transducer models from icefall\n"
      " - Non-streaming Paraformer models from FunASR\n"
      " - EncDecCTCModelBPE models from NeMo\n",
      model_type.c_str());

  exit(-1);
}

}  // namespace sherpa_onnx
