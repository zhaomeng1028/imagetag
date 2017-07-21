#include <jni.h>
#include "com_xiaomi_micloud_imagetagginglib_ImageTaggingAPI.h"
#include "ImageTaggingAPI.h"
#include <string>
#include <vector>
#ifdef __cplusplus
extern "C" {
#endif
	std::string jstring2string(JNIEnv *env, jstring jstr) {
		const char *cstr = env->GetStringUTFChars(jstr, 0);
		std::string str(cstr);
		env->ReleaseStringUTFChars(jstr, cstr);
		return str;
	}

	unsigned char* bytearray2char(JNIEnv *env,jbyteArray array) {
		int len = env->GetArrayLength(array);
		unsigned char* buf = new unsigned char[len];
		env->GetByteArrayRegion(array, 0, len, reinterpret_cast<jbyte*>(buf));
		return buf;
	}

	/*
	* Class:     com_xiaomi_micloud_imagetagginglib_ImageTaggingAPI
	* Method:    Init
	* Signature: (Ljava/lang/String;)I
	*/
	JNIEXPORT jint JNICALL Java_com_xiaomi_micloud_imagetagginglib_ImageTaggingAPI_Init
		(JNIEnv *env, jobject thiz, jstring modelDir){
	        ImageTaggingAPI *imtag = new ImageTaggingAPI();	
		jint result = imtag->Init(jstring2string(env, modelDir));
		jlong imtagAddr = (jlong)imtag;
		jclass clazz = env->GetObjectClass(thiz);
		jmethodID method_setAddr = env->GetMethodID(clazz,"setImageTagScoreAddr","(J)V");
                env->CallVoidMethod(thiz,method_setAddr,imtagAddr);
		return result;
	}

	/*
	* Class:     com_xiaomi_micloud_imagetagginglib_ImageTaggingAPI
	* Method:    GetTags
	* Signature: ([BJLjava/util/ArrayList;)I
	*/
	JNIEXPORT jint JNICALL Java_com_xiaomi_micloud_imagetagginglib_ImageTaggingAPI_GetTags
		(JNIEnv *env, jobject thiz, jbyteArray jpgFileBuffer, jlong bufferSize, jobject imageTagScore){
		unsigned char * chArr = bytearray2char(env,jpgFileBuffer);
		std::vector<ImageTagScore> meanValues;
		jclass objclazz = env->GetObjectClass(thiz);
		jmethodID method_getAddr = env->GetMethodID(objclazz,"getImageTagScoreAddr","()J");
                jlong imtagAddr = env->CallLongMethod(thiz,method_getAddr);
		int result =( (ImageTaggingAPI*)imtagAddr)->GetTags(chArr, bufferSize, &meanValues);
		jclass cls_arraylist = env->GetObjectClass(imageTagScore);
                jmethodID arraylist_add = env->GetMethodID(cls_arraylist,"add","(Ljava/lang/Object;)Z");
		int array_lens = meanValues.size();
		jclass clazz = env->FindClass("com/xiaomi/micloud/imagetagginglib/ImageTagScore");
		for(int i=0;i<array_lens;i++){
			jmethodID construct_user = env->GetMethodID(clazz, "<init>", "()V");
                        jobject obj = env->NewObject(clazz, construct_user,"");
			jmethodID method_setTag = env->GetMethodID(clazz,"setTag","(I)V");
                        env->CallVoidMethod(obj,method_setTag,meanValues[i].tag);
                        jmethodID method_setScore = env->GetMethodID(clazz,"setScore","(D)V");
                        env->CallVoidMethod(obj,method_setScore,meanValues[i].score);
  			env->CallObjectMethod(imageTagScore,arraylist_add,obj);
		} 

		return result;
	}

#ifdef __cplusplus
}
#endif
