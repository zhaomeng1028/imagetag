package com.xiaomi.micloud.imagetagginglib;

import com.sun.javafx.collections.MappingChange;

import java.io.File;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Created by mi on 17-3-31.
 */
public class ImageTaggingService {

    private static String filePath = "/home/mi/CVAlgorithm/newtechsdk/image_tag/linux_1.0.0/models/tagid.txt";
    private static String tagIdFilePath = "/home/mi/CVAlgorithm/newtechsdk/image_tag/linux_1.0.0/models/synset.txt";
    private static String thresholdPath = "/home/mi/CVAlgorithm/newtechsdk/image_tag/linux_1.0.0/models/threshold.txt";

    private static CSVUtils csvUtils = new CSVUtils();
    public static Map<String, String> TagIdMap = new HashMap<String, String>();
    public static Map<String, String> tagScoreMap = new HashMap<String, String>();

    static {
        TagIdMap = getAllLabel();
        tagScoreMap = csvUtils.readLabels(tagIdFilePath, thresholdPath);
    }

    private static Map<String, String> getAllLabel(){
        List<String> content = csvUtils.importCsv(new File(filePath));
        Map<String, String> splitedContent = new HashMap<String, String>();
        for (String s : content) {
            String line = s;
            String[] strs = line.split(",");
            String temp = "";
            if (strs[2].contains("\"")) {
                temp = /*strs[0] + ", " + */strs[1] + ", " + strs[2] + ", " + strs[3];
            }
            else{
                temp = /*strs[0] + ", " + */strs[1] + ", " + strs[2];
            }
            splitedContent.put(strs[0], temp);
        }

        return splitedContent;
    }

}
