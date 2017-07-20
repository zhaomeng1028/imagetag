package com.xiaomi.micloud.imagetagginglib;

import javax.imageio.ImageIO;
import javax.swing.*;
import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import static java.lang.System.exit;

/**
 * Created by mi on 17-3-30.
 */
public class ImageTaggingInterface {

    static {
        System.load("/home/mi/ImageTagging/jni/libImageTagging.so");
    }

    private static String modelDir = "/home/mi/CVAlgorithm/newtechsdk/image_tag/linux_1.0.0/models";

    private static long nativeObj = 0;

    private static ImageTaggingJNI imageTaggingJNI;

    static {
        imageTaggingJNI = new ImageTaggingJNI();
        nativeObj = imageTaggingJNI.createObject();
        if (modelDir != null) {
            int hr = imageTaggingJNI.init(nativeObj, modelDir);
            if(hr == 0)
                System.out.println("模型初始化成功!");
        }
        else{
            System.out.println("路径错误，模型初始化失败！");
            exit(0);
        }
    }

    // 输入为图片路径
    public static List<ImageTagScore> detect(String imagePath, int topK){
        File file = new File(imagePath);
        if(!file.exists()){
            System.out.println("图片文件不存在！");
            return null;
        }
        String ext = imagePath.substring(imagePath.lastIndexOf(".") + 1);
        BufferedImage image = null;
        try {
            image = ImageIO.read(file);
        } catch (IOException e) {
            e.printStackTrace();
        }
        ByteArrayOutputStream baos = new ByteArrayOutputStream(1024);
        try {
            ImageIO.write(image, ext, baos);
        } catch (IOException e) {
            e.printStackTrace();
        }
        byte[] imageBuffer = baos.toByteArray();
        ArrayList<ImageTagScore> imageTagScoreList = imageTaggingJNI.getTags(nativeObj, imageBuffer, imageBuffer.length);
        Collections.sort(imageTagScoreList);
        List<ImageTagScore> result = new ArrayList<ImageTagScore>();
        if(topK < 0)
        {
            topK = imageTagScoreList.size();
        }
        for(int i = 0;i < topK;i++)
        {
            result.add(imageTagScoreList.get(i));
        }

        return result;
    }

    // 输入为图片流
    public static List<ImageTagScore> detect(byte[] imageBuffer, int topK){
        if(imageBuffer == null){
            return null;
        }
        ArrayList<ImageTagScore> imageTagScoreList = imageTaggingJNI.getTags(nativeObj, imageBuffer, imageBuffer.length);
        Collections.sort(imageTagScoreList);
        List<ImageTagScore> result = new ArrayList<ImageTagScore>();
        if(topK < 0)
        {
            topK = imageTagScoreList.size();
        }
        for(int i = 0;i < topK;i++)
        {
            result.add(imageTagScoreList.get(i));
        }

        return result;
    }


}
