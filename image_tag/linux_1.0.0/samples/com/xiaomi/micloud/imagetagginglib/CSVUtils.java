package com.xiaomi.micloud.imagetagginglib;

import com.sun.javafx.collections.MappingChange;
import sun.applet.resources.MsgAppletViewer;

import java.util.HashMap;
import java.util.Map;

import java.io.*;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by mi on 17-3-31.
 */
public class CSVUtils {

    public List<String> importCsv(File file){
        List<String> dataList=new ArrayList<String>();

        BufferedReader br=null;
        try {
            br = new BufferedReader(new FileReader(file));
            String line = "";
            while ((line = br.readLine()) != null) {
                dataList.add(line);
            }
        }catch (Exception e) {
        }finally{
            if(br!=null){
                try {
                    br.close();
                    br=null;
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }

        return dataList;
    }

    public Map<String, String> readLabels(String tagIdFilePath, String thresholdPath){
        Map<String, String> result = new HashMap<String, String>();
        List<String> tagIdList = importCsv(new File(tagIdFilePath));
        List<String> thresholdList = importCsv(new File(thresholdPath));
        if(tagIdList.size() != thresholdList.size()){
            System.out.println("文件格式有问题!");
            return null;
        }

        for(int i = 0;i < tagIdList.size();i++){
            String tag = tagIdList.get(i);
            String tagId = tag.split(" ")[1].trim();
            result.put(tagId, thresholdList.get(i).trim());
        }

        return result;
    }


}
