// ivectorbin/ivector-extract.cc

// Copyright 2013  Daniel Povey

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "gmm/diag-gmm.h"
#include "hmm/transition-model.h"
#include "gmm/full-gmm.h"
#include "hmm/posterior.h"
#include "gmm/am-diag-gmm.h"
#include "ivector/ivector-extractor.h"
#include "thread/kaldi-task-sequence.h"
#include "base/timer.h"

#define MAXBUF 1024
#define PORTNO 1428

using namespace kaldi;
using std::vector;
typedef kaldi::int32 int32;
typedef kaldi::int64 int64;

// This class will be used to parallelize over multiple threads the job
// that this program does.  The work happens in the operator (), the
// output happens in the destructor.
class IvectorExtractTask {
 public:
  IvectorExtractTask(const IvectorExtractor &extractor,
                     std::string utt,
                     const Matrix<BaseFloat> &feats,
                     const Posterior &posterior,
                     BaseFloatVectorWriter *writer,
                     double *tot_auxf_change):
      extractor_(extractor), utt_(utt), feats_(feats), posterior_(posterior),
      writer_(writer), tot_auxf_change_(tot_auxf_change) { }

  void operator () () {
    bool need_2nd_order_stats = false;
    
    IvectorExtractorUtteranceStats utt_stats(extractor_.NumGauss(),
                                             extractor_.FeatDim(),
                                             need_2nd_order_stats);
      
    utt_stats.AccStats(feats_, posterior_);
    
    ivector_.Resize(extractor_.IvectorDim());
    ivector_(0) = extractor_.PriorOffset();

    if (tot_auxf_change_ != NULL) {
      double old_auxf = extractor_.GetAuxf(utt_stats, ivector_);
      extractor_.GetIvectorDistribution(utt_stats, &ivector_, NULL);
      double new_auxf = extractor_.GetAuxf(utt_stats, ivector_);
      auxf_change_ = new_auxf - old_auxf;
    } else {
      extractor_.GetIvectorDistribution(utt_stats, &ivector_, NULL);
    }
  }
  ~IvectorExtractTask() {
    if (tot_auxf_change_ != NULL) {
      double T = TotalPosterior(posterior_);
      *tot_auxf_change_ += auxf_change_;
      KALDI_VLOG(2) << "Auxf change for utterance " << utt_ << " was "
                    << (auxf_change_ / T) << " per frame over " << T
                    << " frames (weighted)";
    }
    // We actually write out the offset of the iVectors from the mean of the
    // prior distribution; this is the form we'll need it in for scoring.  (most
    // formulations of iVectors have zero-mean priors so this is not normally an
    // issue).
    ivector_(0) -= extractor_.PriorOffset();
    KALDI_VLOG(2) << "Ivector norm for utterance " << utt_
                  << " was " << ivector_.Norm(2.0);
    writer_->Write(utt_, Vector<BaseFloat>(ivector_));
  }
 private:
  const IvectorExtractor &extractor_;
  std::string utt_;
  Matrix<BaseFloat> feats_;
  Posterior posterior_;
  BaseFloatVectorWriter *writer_;
  double *tot_auxf_change_; // if non-NULL we need the auxf change.
  Vector<double> ivector_;
  double auxf_change_;
};

int function_ivector(bool compute_objf_change,
                     IvectorEstimationOptions opts,
                     TaskSequencerConfig sequencer_config,
                     std::string ivector_extractor_rxfilename,
                     std::string feature_rspecifier,
                     Posterior post,
                     std::string ivector_wspecifier,
                     IvectorExtractor *pt_extractor) {
  try {
    // g_num_threads affects how ComputeDerivedVars is called when we read the extractor.
    g_num_threads = sequencer_config.num_threads;     

    double tot_auxf_change = 0.0, tot_t = 0.0;
    int32 num_done = 0, num_err = 0;
  
    SequentialBaseFloatMatrixReader feature_reader(feature_rspecifier);
    BaseFloatVectorWriter ivector_writer(ivector_wspecifier);
  
    {
      TaskSequencer<IvectorExtractTask> sequencer(sequencer_config);
      for (; !feature_reader.Done(); feature_reader.Next()) {
        std::string utt = feature_reader.Key();
        const Matrix<BaseFloat> &mat = feature_reader.Value();
        Posterior posterior = post;
        
        if (static_cast<int32>(posterior.size()) != mat.NumRows()) {
          KALDI_WARN << "Size mismatch between posterior " << posterior.size()
                     << " and features " << mat.NumRows() << " for utterance "
                     << utt;
          num_err++;
          continue;
        }

        double *auxf_ptr = (compute_objf_change ? &tot_auxf_change : NULL );

        double this_t = opts.acoustic_weight * TotalPosterior(posterior),
            max_count_scale = 1.0;
        if (opts.max_count > 0 && this_t > opts.max_count) {
          max_count_scale = opts.max_count / this_t;
          KALDI_LOG << "Scaling stats for utterance " << utt << " by scale "
                    << max_count_scale << " due to --max-count="
                    << opts.max_count;
          this_t = opts.max_count;
        }
        ScalePosterior(opts.acoustic_weight * max_count_scale,
                       &posterior);
        // note: now, this_t == sum of posteriors.
        
        sequencer.Run(new IvectorExtractTask(*pt_extractor, utt, mat, posterior,
                                             &ivector_writer, auxf_ptr));
        
        tot_t += this_t;
        num_done++;
      }
      // Destructor of "sequencer" will wait for any remaining tasks.
    }

    KALDI_LOG << "Done " << num_done << " files, " << num_err
              << " with errors.  Total (weighted) frames " << tot_t;
    if (compute_objf_change)
      KALDI_LOG << "Overall average objective-function change from estimating "
                << "ivector was " << (tot_auxf_change / tot_t) << " per frame "
                << " over " << tot_t << " (weighted) frames.";
    return (num_done != 0 ? 0 : 1);
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}

int function_gmm(int32 num_gselect,
                std::string model_filename,
                std::string feature_rspecifier,
                vector<vector<int32> > *pt_gselect,
                DiagGmm *pt_gmm) {
  try {
    DiagGmm &gmm = *pt_gmm;    
    KALDI_ASSERT(num_gselect > 0);
    int32 num_gauss = gmm.NumGauss();
    if (num_gselect > num_gauss) {
      KALDI_WARN << "You asked for " << num_gselect << " Gaussians but GMM "
                 << "only has " << num_gauss << ", returning this many. "
                 << "Note: this means the Gaussian selection is pointless.";
      num_gselect = num_gauss;
    }
    
    double tot_like = 0.0;
    kaldi::int64 tot_t = 0;
    
    SequentialBaseFloatMatrixReader feature_reader(feature_rspecifier);

    int32 num_done = 0, num_err = 0;
    for (; !feature_reader.Done(); feature_reader.Next()) {
      int32 tot_t_this_file = 0; double tot_like_this_file = 0;
      std::string utt = feature_reader.Key();
      const Matrix<BaseFloat> &mat = feature_reader.Value();
      vector<vector<int32> > gselect(mat.NumRows());
      tot_t_this_file += mat.NumRows();
      tot_like_this_file =
            gmm.GaussianSelection(mat, num_gselect, &gselect);
  
      *pt_gselect = gselect;

      if (num_done % 10 == 0)
        KALDI_LOG << "For " << num_done << "'th file, average UBM likelihood over "
                  << tot_t_this_file << " frames is "
                  << (tot_like_this_file/tot_t_this_file);
      tot_t += tot_t_this_file;
      tot_like += tot_like_this_file;
      
      num_done++;
    }

    KALDI_LOG << "Done " << num_done << " files, " << num_err
              << " with errors, average UBM log-likelihood is "
              << (tot_like/tot_t) << " over " << tot_t << " frames.";
    
    if (num_done != 0) return 0;
    else return 1;
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}

int function_fgmm(BaseFloat min_post,
                  std::string model_rxfilename,
                  std::string feature_rspecifier,
                  vector<vector<int32> > gselect,
                  Posterior *pt_post,
                  FullGmm *pt_fgmm) {
  try {
    FullGmm &fgmm = *pt_fgmm;
    
    double tot_loglike = 0.0, tot_frames = 0.0;
    int64 tot_posts = 0;

    SequentialBaseFloatMatrixReader feature_reader(feature_rspecifier);
    int32 num_done = 0, num_err = 0;

    for (; !feature_reader.Done(); feature_reader.Next()) {
      std::string utt = feature_reader.Key();
      const Matrix<BaseFloat> &mat = feature_reader.Value();

      int32 num_frames = mat.NumRows();
      Posterior post(num_frames);
      double this_tot_loglike = 0;
      bool utt_ok = true;
      
      for (int32 t = 0; t < num_frames; t++) {
        SubVector<BaseFloat> frame(mat, t);
        const std::vector<int32> &this_gselect = gselect[t];
        KALDI_ASSERT(!gselect[t].empty());
        Vector<BaseFloat> loglikes;
        fgmm.LogLikelihoodsPreselect(frame, this_gselect, &loglikes);
        this_tot_loglike += loglikes.ApplySoftMax();
        // now "loglikes" contains posteriors.
        if (fabs(loglikes.Sum() - 1.0) > 0.01) {
          utt_ok = false;
        } else {
          if (min_post != 0.0) {
            int32 max_index = 0; // in case all pruned away...
            loglikes.Max(&max_index);
            for (int32 i = 0; i < loglikes.Dim(); i++)
              if (loglikes(i) < min_post)
                loglikes(i) = 0.0;
            BaseFloat sum = loglikes.Sum();
            if (sum == 0.0) {
              loglikes(max_index) = 1.0;
            } else {
              loglikes.Scale(1.0 / sum);
            }
          }
          for (int32 i = 0; i < loglikes.Dim(); i++) {
            if (loglikes(i) != 0.0) {
              post[t].push_back(std::make_pair(this_gselect[i], loglikes(i)));
              tot_posts++;
            }
          }
          KALDI_ASSERT(!post[t].empty());
        }
      }
      if (!utt_ok) {
        KALDI_WARN << "Skipping utterance " << utt
                  << " because bad posterior-sum encountered (NaN?)";
        num_err++;
      } else {
        *pt_post = post;
        num_done++;
        KALDI_VLOG(2) << "Like/frame for utt " << utt << " was "
                      << (this_tot_loglike/num_frames) << " per frame over "
                      << num_frames << " frames.";
        tot_loglike += this_tot_loglike;
        tot_frames += num_frames;
      }
    }

    KALDI_LOG << "Done " << num_done << " files; " << num_err << " had errors.";
    KALDI_LOG << "Overall loglike per frame is " << (tot_loglike / tot_frames)
              << " with " << (tot_posts / tot_frames) << " entries per frame, "
              << " over " << tot_frames << " frames";
    return (num_done != 0 ? 0 : 1);
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}

int main(int argc, char *argv[]) {
  const char *usage = 
      "Usage: ivector-extract-server [options] <gmm-model-in> <fgmm-model-in> <ivector-model-in> <ivector-wspecifier>\n";
  ParseOptions po(usage);
  int32 num_gselect = 50;
  BaseFloat min_post = 0.0;
  bool compute_objf_change = true;
  IvectorEstimationOptions opts;
  TaskSequencerConfig sequencer_config;
  po.Register("n", &num_gselect, "gmm-gselect: Number of Gaussians to keep per frame\n");
  po.Register("min-post", &min_post, "fgmm-gselect-to-post: If nonzero, posteriors below this "
              "threshold will be pruned away and the rest will be renormalized "
              "to sum to one.");
  po.Register("compute-objf-change", &compute_objf_change,
                "ivector-extract: If true, compute the change in objective function from using "
                "nonzero iVector (a potentially useful diagnostic).  Combine "
                "with --verbose=2 for per-utterance information");
  opts.Register(&po);
  sequencer_config.Register(&po);

  po.Read(argc, argv);

  if (po.NumArgs() != 4) {
      po.PrintUsage();
      exit(1);
    }

  std::string gmm_model_in = po.GetArg(1),
    fgmm_model_in = po.GetArg(2),
    ivector_model_in = po.GetArg(3),
    ivector_wspecifier = po.GetArg(4);

  vector<vector<int32> > gselect;
  Posterior post;
  IvectorExtractor extractor;
  DiagGmm gmm;
  FullGmm fgmm;
  Timer server_time = Timer();

  KALDI_LOG << "T before gmm read at " << server_time.Elapsed();
  ReadKaldiObject(gmm_model_in, &gmm);
  KALDI_LOG << "T before fgmm read at " << server_time.Elapsed();
  ReadKaldiObject(fgmm_model_in, &fgmm);
  KALDI_LOG << "T before ivector_read at " << server_time.Elapsed();
  ReadKaldiObject(ivector_model_in, &extractor);
  KALDI_LOG << "T after ivector_read at " << server_time.Elapsed();

  int sockfd;
  struct sockaddr_in server;
  struct sockaddr_in client;
  int client_add;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
    KALDI_ERR << "ERROR opening socket";
  bzero(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(PORTNO);
  if (bind(sockfd, (struct sockaddr*) &server, sizeof(server)) < 0) 
    KALDI_ERR << "ERROR on bind";
  if (listen(sockfd,5) < 0)
    KALDI_ERR << "ERROR on listen";
  KALDI_LOG << "Server started. Waiting.";

  while (1){
    std::string feature_rspecifier;
    char buf_tmp[MAXBUF];
    int clientfd = accept(sockfd, (struct sockaddr*) &client, (socklen_t *) &client_add);
    if (clientfd > 0){
      int n = recv(clientfd, buf_tmp, sizeof(buf_tmp), 0);
      if (n > 0){
        feature_rspecifier.append(buf_tmp, buf_tmp + n);
        if (feature_rspecifier == "quit"){
        	KALDI_LOG << "Server terminating.";
        	break;
        }
        float start_time = server_time.Elapsed();
        KALDI_LOG << "Start at " << start_time;  
        function_gmm(num_gselect, gmm_model_in, feature_rspecifier, &gselect, &gmm);
        KALDI_LOG << "T after gmm " << server_time.Elapsed();
        function_fgmm(min_post, fgmm_model_in, feature_rspecifier, gselect, &post, &fgmm);
        KALDI_LOG << "T after fgmm " << server_time.Elapsed();
        function_ivector(compute_objf_change, opts, sequencer_config,
                        ivector_model_in, feature_rspecifier, post, ivector_wspecifier, &extractor);
        float end_time = server_time.Elapsed();
        KALDI_LOG << "End at " << end_time;
        KALDI_LOG << "Total time " << (end_time - start_time);
        send(clientfd, "Operation completed", 19, 0);
      }
    }
    else sleep(5);
    close(clientfd);
  }
  close(sockfd);
}
