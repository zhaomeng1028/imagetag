package com.xiaomi.micloud.imagetagginglib;

import com.sun.javafx.collections.MappingChange;

import javax.imageio.ImageIO;
import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.util.*;

/**
 * Created by mi on 17-3-30.
 */
public class ImageTaggingTest {

    private static ImageTaggingInterface imageTaggingInterface = new ImageTaggingInterface();
    private static ImageTaggingService imageTaggingService = new ImageTaggingService();

    public static void main(String[] args){
        Map<String, String> TagIdMap = imageTaggingService.TagIdMap;
        Map<String, String> tagScoreMap = imageTaggingService.tagScoreMap;
        String imagePath = "/home/mi/mountains.jpg";
        /*File file = new File(imagePath);
        BufferedImage image = null;
        try {
            image = ImageIO.read(file);
        } catch (IOException e) {
            e.printStackTrace();
        }
        ByteArrayOutputStream baos = new ByteArrayOutputStream(1024);
        try {
            ImageIO.write(image, "jpg", baos);
        } catch (IOException e) {
            e.printStackTrace();
        }
        byte[] imageBuffer = baos.toByteArray();*/
        int topK = -1;
        List<ImageTagScore> imageTagScoreList = imageTaggingInterface.detect(imagePath, topK);
        // System.out.println(imageTagScoreList);

        List<String> predictList = new ArrayList<String>();
        for (ImageTagScore imageTagScore : imageTagScoreList) {
            String labelNameTmp = TagIdMap.get(imageTagScore.getTag() + "");
            //System.out.println(labelNameTmp);
            String labelName = labelNameTmp.split(",")[0];
            double threshold = Double.parseDouble(tagScoreMap.get(labelName));
            if(threshold < imageTagScore.getScore()){
                String predictResult = labelNameTmp + ", prob: " + imageTagScore.getScore();
                predictList.add(predictResult);
            }
        }

        for (String s : predictList) {
            System.out.println(s);
        }

    }



}
