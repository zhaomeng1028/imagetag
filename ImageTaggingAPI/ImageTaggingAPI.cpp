#include "ImageTaggingAPI.h"
#include <vector>
#include <assert.h>
#include <sstream> 
#include <algorithm>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv/cv.hpp>

#include <BufferFile.h>
#include <mxnet/c_predict_api.h>

int debug=0;

ImageTaggingAPI::ImageTaggingAPI()
{
    out = 0;  // alias for void *
    width = 224;
    height =224;
//    width = 299;
//    height =299;
    channels = 3;
}

ImageTaggingAPI::~ImageTaggingAPI()
{
    // Release Predictor
    assert(MXPredFree(out)==0);
}

int ImageTaggingAPI::Init(const std::string & model_dir)
{
    BufferFile json_data(model_dir+"/net.json");
    BufferFile param_data(model_dir+"/net.params");
    std::string synset_dir = model_dir+"/synset.txt";
    std::string tagid_dir = model_dir+"/tagid.txt";
    std::string thresh_dir = model_dir+"/threshold.txt";
    std::string hierarchy_dir = model_dir+"/tags_hierarchy.csv";
    LoadSynset(synset_dir.c_str());
    LoadTagid(tagid_dir.c_str());
    LoadThreshold(thresh_dir.c_str());
    LoadHierarchy(hierarchy_dir.c_str());

    // Parameters
    int dev_type = 1;  // 1: cpu, 2: gpu

    static int dev_id = 0;  // arbitrary.
    int local_devid = 0;
    static pthread_mutex_t mutx = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutx);
    dev_id++;
    local_devid = dev_id; 
    pthread_mutex_unlock(&mutx);

    mx_uint num_input_nodes = 1;  // 1 for feedforward
    const char* input_key[1] = {"data"};
    const char** input_keys = input_key;

    const mx_uint input_shape_indptr[2] = { 0, 4 };
    // ( trained_width, trained_height, channel, num)
    const mx_uint input_shape_data[4] = { 1,
                                        static_cast<mx_uint>(channels),
                                        static_cast<mx_uint>(width),
                                        static_cast<mx_uint>(height) };
    //-- Create Predictor
    MXPredCreate((const char*)json_data.GetBuffer(),
                 (const char*)param_data.GetBuffer(),
                 static_cast<size_t>(param_data.GetLength()),
                 dev_type,
                 local_devid,
                 num_input_nodes,
                 input_keys,
                 input_shape_indptr,
                 input_shape_data,
                 &out);

    return LoadSubModel(model_dir, local_devid);
}

int ImageTaggingAPI::LoadSubModel(const std::string &model_dir, int local_devid)
{
    std::string submodellist_dir = model_dir+"/submodellist.txt";
    std::ifstream fi(submodellist_dir.c_str());
    if ( !fi.is_open() ) {
        //std::cerr << "Error opening file " << submodellist_dir << std::endl;
        fprintf(stderr,"Error opening file %s",submodellist_dir.c_str());
        assert(false);
    }

    std::string synsetid;
    while ( fi >> synsetid ) {
        PredictorHandle submodel_out=0;
	int tagid=synset2tagid[synsetid];

	BufferFile json_data(model_dir+"submodel/"+synsetid+".json");
	BufferFile param_data(model_dir+"submodel/"+synsetid+".params");
	int dev_type = 1;
	mx_uint num_input_nodes = 1;  // 1 for feedforward
	const char* input_key[1] = {"data"};
	const char** input_keys = input_key;
	const mx_uint input_shape_indptr[2] = { 0, 4 };
	// ( trained_width, trained_height, channel, num)
	const mx_uint input_shape_data[4] = { 1,
		                        static_cast<mx_uint>(channels),
		                        static_cast<mx_uint>(width),
		                        static_cast<mx_uint>(height) };

	if (MXPredCreate((const char*)json_data.GetBuffer(),
                 (const char*)param_data.GetBuffer(),
                 static_cast<size_t>(param_data.GetLength()),
                 dev_type,
                 local_devid,
                 num_input_nodes,
                 input_keys,
                 input_shape_indptr,
                 input_shape_data,
                 &submodel_out))
	    return -1;

	submodel[tagid]=submodel_out;
    }
    fi.close();
    return 0;
}

//0: jpg
//1: bmp
//2: png
static int checkImageFormat(const unsigned char *imFileBuf, int nBufSize)
{
  int nFormat = 0;
  if (imFileBuf[0] == 0xff && imFileBuf[1] == 0xD8)//&&imFileBuf[nBufSize-2]==0xff &&imFileBuf[nBufSize-1]==0xD9)
  {
    nFormat = 0;   //jpg
  }
  else if (imFileBuf[0] == 0x4D && imFileBuf[1] == 0x42)
  {
    nFormat = 1;   //bmp
  }
  else if (imFileBuf[0] == 0x89 && imFileBuf[1] == 0x50 && imFileBuf[2] == 0x4E && imFileBuf[3] == 0x47 &&
    imFileBuf[4] == 0x0D && imFileBuf[5] == 0x0A && imFileBuf[6] == 0x1A && imFileBuf[7] == 0x0A)
  {
    nFormat = 2;  //png
  }
  else if (((imFileBuf[0] == 'I' && imFileBuf[1] == 'I') || (imFileBuf[0] == 'M' && imFileBuf[1] == 'M')) &&
    imFileBuf[2] == 42)
  {
    nFormat = 2;  //tif
  }
  else
  {
    nFormat = -1;
  }

  return nFormat;
}

int ImageTaggingAPI::GetTags(const unsigned char *jpg_file_buffer, 
                             long buffer_size, 
                             std::vector<ImageTagScore>* result)
{
//    std::vector<unsigned char> jpg_data(jpg_file_buffer, buffer_size);
    std::vector<unsigned char> jpg_data;
    for(int i=0; i<buffer_size; i++)
    {
        jpg_data.push_back(jpg_file_buffer[i]);
    }

    cv::Mat im = cv::imdecode(cv::Mat(jpg_data), cv::IMREAD_COLOR);

    if (im.empty()){
        fprintf(stderr,"Can not load image.\n");
        return -1;
    }

    // Just a big enough memory 1000x1000x3
    int image_size = width*height*channels;
    std::vector<mx_float> image_data = std::vector<mx_float>(image_size);

    //-- Read Mean Data
    try
    {
        GetMeanFile(im, image_data.data(), channels, cv::Size(width, height));
    }
    catch(cv:: Exception & e)
    {
        const char* err_msg = e.what();
        //std::cout<<"Exception caught: " << err_msg << std::endl;
        printf("Exception caught: %s",err_msg);
        return -1; 
    }

    //-- Set Input Image
    if(MXPredSetInput(out, "data", image_data.data(), image_size))
        return -1;

    //-- Do Predict Forward
    if(MXPredForward(out))
        return -1;

    mx_uint output_index = 0;

    mx_uint *shape = 0;
    mx_uint shape_len;

    //-- Get Output Result
    if(MXPredGetOutputShape(out, output_index, &shape, &shape_len))
        return -1;

    size_t size = 1;
    for (mx_uint i = 0; i < shape_len; ++i) size *= shape[i];

    std::vector<float> data(size);

    if(MXPredGetOutput(out, output_index, &(data[0]), size))
        return -1;

 
    /*float total_accuracy = 0.0;
    for ( int i = 0; i < static_cast<int>(data.size()); i++ ) {
        if( synset2tagid.find( synset[i] )!=synset2tagid.end() )
        {
            total_accuracy += data[i];
        }
    }*/

    int index=0;
    std::vector<ImageTagScore> curr_result;
    for ( int i = 0; i < static_cast<int>(data.size()); i++ ) {
        if( synset2tagid.find( synset[i] )!=synset2tagid.end() )
        {
            ImageTagScore its;
            //its.score = data[i]/total_accuracy;
	    its.score = data[i];
            its.tag = synset2tagid[ synset[i] ];

            curr_result.push_back(its);
	    tagid2index[its.tag]=index;
	    index++;
        }
    }


    UpdateTagBySubModel(curr_result,image_data,image_size);

    UpdateTagByHierarchy(&curr_result);

    if (debug){
	for (std::vector<ImageTagScore>::iterator iter = curr_result.begin();iter<curr_result.end();iter++){
	if(iter->score>=threshold[iter->tag]){
	    printf("Predict: id = %d, score = %.8f, thresh=%.6f, parent=%d\n",
	iter->tag, iter->score, threshold[iter->tag], hierarchy[iter->tag]);
	}
	}
    }
 
    result->insert(result->end(),curr_result.begin(),curr_result.end());

/*
    static int i=0;
    i++;
    char imname[20] = {'\0'};
    sprintf(imname, "%i.jpg", i);
    std::ostringstream ss;
    ss<<best_accuracy;
    cv::putText(im, synset[best_idx]+ss.str(), cv::Point(50,50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255,0,0));
    cv::imwrite(imname, im);
*/
    return 0;
}

int ImageTaggingAPI::UpdateTagBySubModel(std::vector<ImageTagScore> &result, std::vector<mx_float> &image_data, int image_size)
{
    std::map<int,PredictorHandle>::iterator iter;
    for (iter=submodel.begin();iter!=submodel.end();iter++){
	int curr_tagid=iter->first;
	if (result[tagid2index[curr_tagid]].score>=threshold[curr_tagid]){
	    PredictorHandle curr_model=iter->second;
	    printf("Run submodel for id[%d]\n",curr_tagid);

	    //-- Set Input Image
	    if(MXPredSetInput(curr_model, "data", image_data.data(), image_size))
		return -1;
	    //-- Do Predict Forward
	    if(MXPredForward(curr_model))
		return -1;
	    mx_uint output_index = 0;
	    mx_uint *shape = 0;
	    mx_uint shape_len;
	    //-- Get Output Result
	    if(MXPredGetOutputShape(curr_model, output_index, &shape, &shape_len))
		return -1;
	    size_t size = 1;
	    for (mx_uint i = 0; i < shape_len; ++i) size *= shape[i];
	    std::vector<float> data(size);
	    if(MXPredGetOutput(curr_model, output_index, &(data[0]), size))
		return -1;

	    double score=data[1];
	    if (debug) printf("id[%d] submodel score: %f\n",curr_tagid,score);
	    if (score<0.5){
		if (debug) printf("Change id[%d] score from [%f] to 0\n",curr_tagid,result[tagid2index[curr_tagid]].score);
		result[tagid2index[curr_tagid]].score=0;
	    }
	}
    }
    return 0;
}


void ImageTaggingAPI::UpdateTagByHierarchy(std::vector<ImageTagScore>* result)
{
    for (int i=0; i<static_cast<int>(result->size()); i++ ){
	std::vector<ImageTagScore>::iterator curr_iter=result->begin()+i;
	if(curr_iter->score>=threshold[curr_iter->tag]){
	    while(hierarchy[curr_iter->tag]!=-1){
		std::vector<ImageTagScore>::iterator parent_iter=result->begin()+tagid2index[hierarchy[curr_iter->tag]];
		parent_iter->score=std::max(parent_iter->score,threshold[parent_iter->tag]);
		curr_iter=parent_iter;
	    }
	}
    }
}
   
void ImageTaggingAPI::LoadTagid(const char *filename)
{
    std::ifstream infile( filename );

    //read the head line
    std::string s0;
    getline( infile, s0 );
    while (infile)
    {
	 std::string s;
         std::vector <std::string> record;
         if (!getline( infile, s )) break;
         std::istringstream ss( s );
     
         while (ss)
         {
              std::string s;
              if (!getline( ss, s, ',' )) break;
              record.push_back( s );
         }
      
         std::stringstream ss1(record[0]); 
         int tagid; 
         ss1>>tagid;
         std::string synsetid = record[1];
         synset2tagid[synsetid] = tagid;
    }
    if (!infile.eof())
    {
         //std::cerr << "Fooey!\n";
         fprintf(stderr,"Fooey!\n");
         
    }
}

void ImageTaggingAPI::LoadSynset(const char *filename) {
    std::ifstream fi(filename);
    if ( !fi.is_open() ) {
        //std::cerr << "Error opening file " << filename << std::endl;
        fprintf(stderr,"Error opening file %s",filename);
        assert(false);
    }

    std::string id, synsetid;
    while ( fi >> id ) {
        getline(fi, synsetid);
        synset.push_back(synsetid.substr(1) );
    }
    fi.close();
}

void ImageTaggingAPI::LoadThreshold(const char *filename) {
    std::ifstream fi(filename);
    if ( !fi.is_open() ) {
        //std::cerr << "Error opening file " << filename << std::endl;
        fprintf(stderr,"Error opening file %s",filename);
        assert(false);
    }
    
    int tagid;
    double thresh;
    while ( fi >> tagid ) {
        fi >> thresh;
        threshold[tagid]=thresh;
    }
    fi.close();
}

void ImageTaggingAPI::LoadHierarchy(const char *filename)
{
    std::ifstream infile( filename );

    //read the head line
    std::string s0;
    getline( infile, s0 );

    while (infile)
    {
         std::string s;
         std::vector <std::string> record;
         if (!getline( infile, s )) break;
         std::istringstream ss( s );
     
         while (ss)
         {
              std::string s;
              if (!getline( ss, s, ',' )) break;
              record.push_back( s );
         }
      
         std::stringstream ss1(record[0]); 
         int tagid; 
         ss1>>tagid;
	 //std::stringstream ss2(record.back()); 
	 std::stringstream ss2(*(record.end()-3)); 
         int tagidp; 
         ss2>>tagidp;
         hierarchy[tagid] = tagidp;
    }
    if (!infile.eof())
    {
         //std::cerr << "Fooey!\n";
         fprintf(stderr,"Fooey!\n");
    }
}

void ImageTaggingAPI::GetMeanFile(cv::Mat im_ori, mx_float* image_data,
                const int channels, const cv::Size resize_size) {
    // Read all kinds of file into a BGR color 3 channels image

    cv::Mat im;
    //cv::resize(im_ori, im, resize_size);

    //center crop instead of resize to 224*224
    cv::Mat res;
    int new_size = 256;

    if (im_ori.rows > im_ori.cols){
        cv::resize(im_ori,res,cv::Size(new_size,im_ori.rows * new_size / im_ori.cols));
    }else{
        cv::resize(im_ori,res,cv::Size(im_ori.cols * new_size / im_ori.rows,new_size));
    } 
    int y = res.rows - resize_size.height;
    int x = res.cols - resize_size.width;
    y/=2;
    x/=2;
    //printf("%resize row=%d,resize col=%d,y=%d,x=%d\n",res.rows,res.cols,y,x);
    cv::Rect roi(x,y,resize_size.width,resize_size.height);
    im = res(roi);


    // Better to be read from a mean.nb file
//    float mean_r = 123.68;
//    float mean_g = 116.779;
//    float mean_b = 103.939;
//    float mean = 128;
    float mean = 0;

    int size = im.rows * im.cols * channels;

    mx_float* ptr_image_r = image_data;
    mx_float* ptr_image_g = image_data + size / 3;
    mx_float* ptr_image_b = image_data + size / 3 * 2;

    for (int i = 0; i < im.rows; i++) {
        uchar* data = im.ptr<uchar>(i);

        for (int j = 0; j < im.cols; j++) {
            if (channels > 1)
            {
//                mx_float b = (static_cast<mx_float>(*data++) - mean_b);
//                mx_float g = (static_cast<mx_float>(*data++) - mean_g);
//                mx_float b = (static_cast<mx_float>(*data++) - mean)/128.0;
//                mx_float g = (static_cast<mx_float>(*data++) - mean)/128.0;
                mx_float b = (static_cast<mx_float>(*data++) - mean);
                mx_float g = (static_cast<mx_float>(*data++) - mean);
                *ptr_image_g++ = g;
                *ptr_image_b++ = b;
            }

//            mx_float r = (static_cast<mx_float>(*data++) - mean_r);
//            mx_float r = (static_cast<mx_float>(*data++) - mean)/128.0;
            mx_float r = (static_cast<mx_float>(*data++) - mean);
            *ptr_image_r++ = r;

        }
    }
}

