package com.xiaomi.micloud.imagetagginglib;

import java.util.ArrayList;
import java.util.List;

/**
 * Created by mi on 17-3-29.
 */
public class ImageTaggingJNI {

    // 创建object
    public static native long createObject();

    // modelDir: 模型所在目录
    public static native int init(long nativeObject, String modelDir);

    // 返回的是结果
    public static native ArrayList<ImageTagScore> getTags(long nativeObject, byte[] imageBuffer, long bufferSize);

    // 释放object
    public static native void releaseObject(long nativeObject);
}
