#ifndef __IMAGE_TAGGING_API_H__
#define __IMAGE_TAGGING_API_H__

//#include <opencv2/core/core.hpp>
//#include <opencv2/highgui/highgui.hpp>
//#include <opencv/cv.hpp>
//#include <mxnet/c_predict_api.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

namespace cv{
    class Mat;
    template <typename T>
        class Size_;
    typedef Size_<int> Size2i;
    typedef Size2i Size;
}
typedef void *PredictorHandle;
typedef float mx_float;

// This struct represents one tag and its score.
struct ImageTagScore {
  // The tag index.
  int tag;
  // The score/confidence.
  double score;
};

class ImageTaggingAPI {
public:
    ImageTaggingAPI();
    ~ImageTaggingAPI();

    // Inits the instance. Returns 0 on success.
    int Init(const std::string &model_dir);

    // Runs image tagging algorithm on the given image. The result is put into the given vector.
    // Returns 0 on success.
    int GetTags(const unsigned char *jpg_file_buffer, long buffer_size, 
                std::vector<ImageTagScore>* result);

private:
    void GetMeanFile(cv::Mat im, mx_float* image_data,
                     const int channels, const cv::Size resize_size);
    void LoadSynset(const char *filename);
    void LoadTagid(const char *filename);

    void LoadThreshold(const char *filename);
    void LoadHierarchy(const char *filename);
    void UpdateTagByHierarchy(std::vector<ImageTagScore>* result);

    int LoadSubModel(const std::string &model_dir, int local_devid);
    int UpdateTagBySubModel(std::vector<ImageTagScore> &result, std::vector<mx_float> &image_data, int image_size);

    PredictorHandle out;  // alias for void *

    // input size of the image tagging model
    int width;
    int height;
    int channels;

    std::vector<std::string> synset; // synset id used by imageset
    std::map<std::string,int> synset2tagid; // tagid is used by xiaomi_yun

    std::map<int,double> threshold; //map tagid to threshold
    std::map<int,int> hierarchy; //map tagid to its parent tagid
    std::map<int,int> tagid2index; //map tagid to index

    std::map<int,PredictorHandle> submodel; //map tagid to submodel predictor
};

#endif  // __IMAGE_TAGGING_API_H__

