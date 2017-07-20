package com.xiaomi.micloud.imagetagginglib;

/**
 * Created by mi on 17-3-29.
 */
public class ImageTagScore implements Comparable {
    public int tag;

    public double score;

    public int getTag() {
        return tag;
    }

    public void setTag(int tag) {
        this.tag = tag;
    }

    public double getScore() {
        return score;
    }

    public void setScore(double score) {
        this.score = score;
    }

    @Override
    public int compareTo(Object o) {
        ImageTagScore imageTagScore = (ImageTagScore) o;
        if(this.score > imageTagScore.score){
            return -1;
        }
        else
            return 1;
    }

    @Override
    public String toString() {
        return "[tagId: " + tag + ", score: " + score + "] ";
    }
}
