#include"yolo.h"
using namespace std;
using namespace cv;
using namespace cv::dnn;

bool Yolo::readModel(Net& net, string& netPath, bool isCuda = true)
{
    try
    {
        net = readNet(netPath);
    }
    catch (const std::exception&)
    {
        return false;
    }
    //cuda
    if (isCuda)
    {
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA_FP16);
    }
    //cpu
    else
    {
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }
    return true;
}

bool Yolo::Detect(Mat& SrcImg, Net& net, vector<Output>& output)
{
    Mat blob;
    int col = SrcImg.cols;
    int row = SrcImg.rows;
    int maxLen = MAX(col, row);
    Mat netInputImg = SrcImg.clone();
    if (maxLen > 1.2 * col || maxLen > 1.2 * row)
    {
        Mat resizeImg = Mat::zeros(maxLen, maxLen, CV_8UC3);
        SrcImg.copyTo(resizeImg(Rect(0, 0, col, row)));
        netInputImg = resizeImg;
    }
    vector<Ptr<Layer> > layer;
    vector<string> layer_names;
    layer_names = net.getLayerNames();
    blobFromImage(netInputImg, blob, 1 / 255.0, cv::Size(netWidth, netHeight), cv::Scalar(0, 0, 0), true, false);
    /*������������һ�²�����
    netInputImg����������Ҫ����Ԥ���������ͼ����Ӧ����cv::Mat���ͣ�����OpenCV�б�ʾͼ��ľ������ݽṹ��
    blob���������ڴ洢Ԥ����ͼ������blob��Blob��һ����ά���飬���������ѧϰ����и�Ч�ش洢�ʹ���������ݡ�
    1 / 255.0������������������ӣ����ڶ�ͼ�������ֵ���й�һ��������255.0������ֵ���ŵ�0��1�ķ�Χ�ڣ����������糣�õĹ�һ��������
    cv::Size(netWidth, netHeight)���������ָ��������ͼ�񽫱�������С�Ŀռ�ߴ硣netWidth��netHeight��ʾ����������������ͼ��Ŀ�Ⱥ͸߶ȡ�
    cv::Scalar(0, 0, 0)�����Ǵ�ͼ���ÿ��ͨ���м�ȥ�ľ�ֵ������������£�(0, 0, 0)��ʾ��ÿ��ͨ���м�ȥ�㣬�Ӷ���ͼ����С�
    true�����������ʾ�Ƿ񽻻�����ͼ�����ɫͨ����OpenCV��BGR�����̺죩��ʽ��ȡͼ�񣬶�������ѧϰģ������ͼ����RGB������������ʽ���������������Ϊtrue����Ӧ�ؽ���ͨ����
    false�����������ʾ�Ƿ�ü�������С��ͼ�������������ȷ����ȷƥ������Ŀռ�ߴ硣��������Ϊfalse��ʾ��ʹ������������С��ͼ�񣬲����вü���
    ����������ý�������ͼ����й�һ����������С����ֵ������ͨ����������������洢��һ���������������������blob�С�*/
    net.setInput(blob);
    std::vector<cv::Mat> netOutputImg;
    net.forward(netOutputImg, net.getUnconnectedOutLayersNames());
    std::vector<int> classIds;//���id����
    std::vector<float> confidences;//���ÿ��id��Ӧ���Ŷ�����
    std::vector<cv::Rect> boxes;//ÿ��id���ο�
    float ratio_h = (float)netInputImg.rows / netHeight;
    float ratio_w = (float)netInputImg.cols / netWidth;
    int net_width = className.size() + 5;  //������������������+5
    for (int stride = 0; stride < strideSize; stride++) {    //stride
        float* pdata = (float*)netOutputImg[stride].data;
        int grid_x = (int)(netWidth / netStride[stride]);
        int grid_y = (int)(netHeight / netStride[stride]);
        for (int anchor = 0; anchor < 3; anchor++) {	//anchors
            const float anchor_w = netAnchors[stride][anchor * 2];
            const float anchor_h = netAnchors[stride][anchor * 2 + 1];
            for (int i = 0; i < grid_y; i++) {
                for (int j = 0; j < grid_x; j++) {
                    float box_score = sigmoid_x(pdata[4]); ;//��ȡÿһ�е�box���к���ĳ������ĸ���
                    if (box_score >= boxThreshold) {
                        cv::Mat scores(1, className.size(), CV_32FC1, pdata + 5);
                        Point classIdPoint;
                        double max_class_socre;
                        minMaxLoc(scores, 0, &max_class_socre, 0, &classIdPoint);
                        max_class_socre = sigmoid_x(max_class_socre);
                        if (max_class_socre >= classThreshold) {
                            float x = (sigmoid_x(pdata[0]) * 2.f - 0.5f + j) * netStride[stride];  //x
                            float y = (sigmoid_x(pdata[1]) * 2.f - 0.5f + i) * netStride[stride];   //y
                            float w = powf(sigmoid_x(pdata[2]) * 2.f, 2.f) * anchor_w;   //w
                            float h = powf(sigmoid_x(pdata[3]) * 2.f, 2.f) * anchor_h;  //h
                            int left = (int)(x - 0.5 * w) * ratio_w + 0.5;
                            int top = (int)(y - 0.5 * h) * ratio_h + 0.5;
                            classIds.push_back(classIdPoint.x);
                            confidences.push_back(max_class_socre * box_score);
                            boxes.push_back(Rect(left, top, int(w * ratio_w), int(h * ratio_h)));
                        }
                    }
                    pdata += net_width;//��һ��
                }
            }
        }
    }
    /*��δ�����һ��Ŀ�����㷨����Ҫѭ�������ڴ���������������ȡ��⵽��Ŀ���������Ϣ��
for (int stride = 0; stride < strideSize; stride++)������ÿ��stride����������ѭ��������
float* pdata = (float*)netOutputImg[stride].data;����ȡ��ǰstride�����������������ָ�롣
int grid_x = (int)(netWidth / netStride[stride]);�����㵱ǰstride��x���������������
int grid_y = (int)(netHeight / netStride[stride]);�����㵱ǰstride��y���������������
for (int anchor = 0; anchor < 3; anchor++)������ÿ��anchor��ê�򣩣�ѭ��������
const float anchor_w = netAnchors[stride][anchor * 2];����ȡ��ǰstride��anchor�Ŀ�ȡ�
const float anchor_h = netAnchors[stride][anchor * 2 + 1];����ȡ��ǰstride��anchor�ĸ߶ȡ�
for (int i = 0; i < grid_y; i++)������ÿ��y�����ϵ�����ѭ��������
for (int j = 0; j < grid_x; j++)������ÿ��x�����ϵ�����ѭ��������
float box_score = sigmoid_x(pdata[4]);����ȡ��ǰ�����Ŀ���÷֣�box_score����ʹ��sigmoid�����Ե÷ֽ��д���
if (box_score >= boxThreshold)�����Ŀ���÷ִ��ڵ����趨����ֵ��boxThreshold����ִ�����²�����
cv::Mat scores(1, className.size(), CV_32FC1, pdata + 5);������һ��cv::Mat�������ڴ洢��ǰ��������÷֡�
Point classIdPoint;������һ��Point�������ڴ洢������÷ֵ�������Ϣ��
double max_class_score;������һ�����������ڴ洢������÷ֵ�ֵ��
minMaxLoc(scores, 0, &max_class_score, 0, &classIdPoint);���ҵ����÷��е����ֵ�������ꡣ
max_class_score = sigmoid_x(max_class_score);����������÷ֽ���sigmoid��������
if (max_class_score >= classThreshold)�����������÷ִ��ڵ����趨����ֵ��classThreshold����ִ�����²�����
float x = (sigmoid_x(pdata[0]) * 2.f - 0.5f + j) * netStride[stride];������Ŀ����x���ꡣ
float y = (sigmoid_x(pdata[1]) * 2.f - 0.5f + i) * netStride[stride];������Ŀ����y���ꡣ
float w = powf(sigmoid_x(pdata[2]) * 2.f, 2.f) * anchor_w;������Ŀ���Ŀ�ȡ�
float h = powf(sigmoid_x(pdata[3]) * 2.f, 2.f) * anchor_h;������Ŀ���ĸ߶ȡ�
int left = (int)(x - 0.5 * w) * ratio_w + 0.5;������Ŀ������߽硣
int top = (int)(y - 0.5 * h) * ratio_h + 0.5;������Ŀ�����ϱ߽硣
classIds.push_back(classIdPoint.x);����������÷ֵ�����ʶ��classIdPoint.x����ӵ�classIds�����С�
confidences.push_back(max_class_score * box_score);����������÷ֺ�Ŀ���÷ֵĳ˻���ӵ�confidences�����С�
boxes.push_back(Rect(left, top, int(w * ratio_w), int(h * ratio_h)));����������Ŀ��������ӵ�boxes�����С�
pdata += net_width;��������ָ���ƶ�����һ�С�
����Ԥ�������趨����ֵ����ȡ��⵽��Ŀ���������Ϣ���������Ǵ洢����Ӧ�������У��Ա�����Ĵ������ʾ��*/
    //ִ�з�����������������нϵ����Ŷȵ������ص���NMS��
    vector<int> nms_result;
    NMSBoxes(boxes, confidences, nmsScoreThreshold, nmsThreshold, nms_result);
    /*����ִ�з�������ƣ�Non-Maximum Suppression�������������ǽ���һ�´���Ĺ��ܣ�
boxes������һ����������Ŀ���ľ����б�
confidences��������ÿ��Ŀ�����ص����Ŷȣ�confidence���б�
nmsScoreThreshold������һ����ֵ������ɸѡ�����Ŷȵ��ڸ���ֵ��Ŀ���
nmsThreshold�����Ƿ�������Ƶ��ص���ֵ��������ȷ������Ŀ�����Ϊ�ص��ĳ̶ȡ�
nms_result����������Ľ���б����ڴ洢ͨ����������Ʋ���ѡ�е�Ŀ����������
�ú����������Ƕ�boxes�е�Ŀ�����з�������ƴ���ȥ���ص��Ƚϸ������ŶȽϵ͵�Ŀ��򣬱������ŶȽϸ����ص��Ƚϵ͵�Ŀ���
ִ�������д����nms_result�б�����ͨ����������Ʋ���ѡ�е�Ŀ������������Щ�����������ڻ�ȡѡ�е�Ŀ���������Ϣ��*/
    for (int i = 0; i < nms_result.size(); i++) {
        int idx = nms_result[i];
        Output result;
        result.id = classIds[idx];
        result.confidence = confidences[idx];
        result.box = boxes[idx];
        output.push_back(result);
    }

    if (output.size())
        return true;
    else
        return false;
}


void Yolo::drawPred(Mat& img, vector<Output> result, vector<Scalar> color)
{
    personnum = result.size();
    for (int i = 0; i < result.size(); i++)
    {
        int left, top;
        left = result[i].box.x;
        top = result[i].box.y;
        int color_num = i;
        rectangle(img, result[i].box, color[result[i].id], 2, 8);

        
        string label = className[result[i].id] + ":0." + to_string(int(result[i].confidence*100));
        Yolo::emotion[result[i].id] += 1;

        int baseLine;
        Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
        top = max(top, labelSize.height);
        putText(img, label, Point(left, top), FONT_HERSHEY_SIMPLEX, 1, color[result[i].id], 2);
    }
    /*ʹ����ȡ��Ŀ�������ͼ����л��ƺͱ�ǣ��Լ���ÿ��Ŀ����������ͳ�ƣ�
for (int i = 0; i < result.size(); i++)������ÿ����ȡ��Ŀ���ѭ��������
int left, top; left = result[i].box.x; top = result[i].box.y;����ȡ��ǰĿ�������Ͻ����ꡣ
int color_num = i;��ΪĿ���ѡ��һ����ɫ��š�
rectangle(img, result[i].box, color[result[i].id], 2, 8);����ͼ���ϻ���Ŀ���ʹ��ָ����ɫ���߿�
string label = className[result[i].id] + ":0." + to_string(int(result[i].confidence*100));������Ŀ���ı�ǩ������������ƺ����Ŷȡ�
Yolo::emotion[result[i].id] += 1;����Ŀ����������ͳ�ƣ������ļ�����1��
int baseLine; Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);�������ǩ�ı��Ĵ�С��
top = max(top, labelSize.height);��ȷ����ǩ������Ŀ����Ϸ���
putText(img, label, Point(left, top), FONT_HERSHEY_SIMPLEX, 1, color[result[i].id], 2);����ͼ���ϻ��Ʊ�ǩ�ı���ָ��λ�á����塢��ɫ���߿�
����ȡ��Ŀ��������ͼ���ϣ�����ÿ��Ŀ����Ϸ��������ǩ��ͬʱ��������ÿ������Ŀ�����м���ͳ�ƣ��Ա���������ʹ���*/

    if (window) {
        imshow("emotion", img);
        checkwindow = 0;
    }
    else if(checkwindow==0){
        cv::destroyWindow("emotion");
        checkwindow = 1;
    };
}
