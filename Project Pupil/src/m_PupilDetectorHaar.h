// Zhonghua Wan @ 20190408
// Class for pupil features extracion
// modify time: 2019.4.8
// create time: 2019.1.19

//------------------------------------------------
//					an example
//------------------------------------------------
//HaarParams params;
//params.width_min = 51;
//params.width_max = 300;
//params.width_step = 4;
//params.ratio_min = 2;
//params.ratio_max = 3;
//measureTime([&]() {
//	haar.Detect(img.gray, params);
//});
//
//haar.draw(img.BGR);
//imshow("image", img.BGR);
//waitKey(30);


#ifndef M_PupilDetectorHaar_H_
#define M_PupilDetectorHaar_H_

#include <opencv2/opencv.hpp>

#include <my_lib.h>

//#define UNIT_TEST
//#define HAAR_TEST

//using namespace cv;
using namespace std;
using cv::Mat;
using cv::Mat_;
using cv::Point; 
using cv::Scalar;

namespace myColor
{
	const Scalar red = Scalar(0, 0, 255);
	const Scalar blue = Scalar(255, 0, 0);
};

enum HaarShape {OUTER_SQUARE, OUTER_HORIZONTAL}; // outer rectangle shape


class HaarParams
{
public:
	HaarParams() : initRectFlag(false), squareHaarFlag(true),
		outer_ratio(1.42), kf(1),
		width_min(31), width_max(120), wh_step(4), xy_step(4), roi(Rect(0, 0, 0, 0)), 
		init_rect(Rect(0, 0, 0, 0)), mu_inner(50), mu_outer(200) 
	{};

	HaarShape outer_shape;
	bool squareHaarFlag;
	double outer_ratio; // outer_rect ratio
	double kf;


	// width: --> pupil_rect
	int width_min; //Ĭ�ϲ�ʹ��ԭʼ�ֱ��ʵ�width
	int width_max;
	int wh_step; //wh�ļ������

	int xy_step; //xy�ļ������
	Rect roi; //inner rect �ķ�Χ�������ܳ����÷�Χ����Ȼ(x,y)������ΧҪ����width

	bool initRectFlag;
	Rect init_rect; //init rect�ķ�Χ
	double mu_inner;
	double mu_outer;
};


/* Detects pupil features using Haar detector.
@param imgGray input image: UINT8 [0, 255]. If imgGray is float [0,1], we can add a function to
	transfrom it to [0,255]. But it is not recommended, as [0,255] has better significance than [0,1].
*/
// TODO(zhwan): whether to use float image to compute
class PupilDetectorHaar
{
public:
	PupilDetectorHaar() :pupil_rect_coarse_(Rect()), outer_rect_coarse_(Rect()),
		max_response_coarse_(-255), iterate_count_(0) {};

	PupilDetectorHaar(const Mat &imgGray, const HaarParams& params) : PupilDetectorHaar()
	{
		detect(imgGray, params);
	}

	void detect(const Mat &img_gray, const HaarParams& params);
	void detectIris(const Mat &img_gray, const HaarParams& params);

	void initialSearchRange(const Mat& img_down);
	void coarseDetection(const Mat& img_down);


	/** ��outer_rect�ڲ���һ��Ѱ�Ҹ��ŵ�pupil_rect
	*/
	void detectToFine()
	{
		inner_bII0 = getBlockIntegral(integral_img, pupil_rect_coarse_);

		//�����޶���ϸRect�ķ�Χ
		double rangeRatio = 2.2;
		double outInlierRatio = 2;//1.41�ƺ�̫С
		int xystep = 4;

		Rect xyrange_rect = rectScale(pupil_rect_coarse_, rangeRatio)&Rect(0, 0, integral_img.cols, integral_img.rows);
		int Wm = xyrange_rect.width;
		int Hm = xyrange_rect.height;
		int Xm = xyrange_rect.x;
		int Ym = xyrange_rect.y;

		int wmin = max(31, pupil_rect_coarse_.width / 2);
		int wmax = rangeRatio * pupil_rect_coarse_.width;
		max_response_fine_ = -10000;
		for (int w = wmin; w <= wmax; w += xystep)
		{
			int hmin = max(31, w / 2);
			for (int h = hmin; h <= w; h += xystep)
			{
				int xmin = Xm;
				int ymin = Ym;
				int xmax = Xm + Wm - 1 - w;
				int ymax = Ym + Hm - 1 - h;

				for (int x = xmin; x <= xmax; x += 4)
					for (int y = ymin; y <= ymax; y += 4)
					{
						Rect pupil_rect0(x, y, w, h);
						Rect outer_rect0 = Rect(x - (outInlierRatio - 1)*w / 2, y - (outInlierRatio - 1)*h / 2, \
							w*outInlierRatio, h*outInlierRatio)&outer_rect_coarse_;
						double mu_inner, mu_outer;
						auto f = getResponseValue(integral_img, pupil_rect0, outer_rect0, mu_inner, mu_outer, 0);
						//����ԭʼ��response���񲻺���
						if (max_response_fine_ < f)
						{
							max_response_fine_ = f;
							pupil_rect_fine_ = pupil_rect0;
							outer_rect_fine_ = outer_rect0;
						}
					}//end for x
			}//end for h
		}//end for w
	}


	/*
	���ۣ�2020.5.3
	��ͼ�����������inner outer�Աȶȵͣ�������ֵ��������Ч��
	*/
	void fineDetection(const Mat& img_down)
	{
		double rangeRatio = 1.42;//ratio=2 is too large
		Rect base_rect = rectScale(pupil_rect_coarse_, rangeRatio)&Rect(0, 0, img_down.cols, img_down.rows);
		Mat img = img_down(base_rect);
		Mat img_bw;

		//��ֵ�����ԣ������ֵ�����ֵ
		//double thresh = (mu_outer_ - mu_inner_) / 2;
		double thresh = mu_inner_; //��������ֵ��Ϊ��ֵ���������
		threshold(img, img_bw, thresh, 255, cv::THRESH_BINARY_INV);
		//threshold(img, img_bw, thresh, 255, cv::THRESH_OTSU | cv::THRESH_BINARY_INV);

		Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, Size(5, 5));
		cv::dilate(img_bw, img_bw, kernel, Point(-1, -1), 1);

		//ȷ������pupil������
		Mat labels, stats, centroids;
		cv::connectedComponentsWithStats(img_bw, labels, stats, centroids);
		Mat area = stats.col(cv::CC_STAT_AREA); //����������Ԫ��
		area.at<int>(0, 0) = 0;
		Mat stats_t;
		for (int i = 1; i < area.rows; i++)//�����ǵ�һ��Ԫ�أ���������ͼ
		{
			if (area.at<int>(i) > 0.04*img_bw.cols*img_bw.rows) //0.04=1/25
				stats_t.push_back(stats.row(i));
		}
		if (stats_t.rows == 1)
		{
			pupil_rect_fine_ = Rect(stats_t.at<int>(0, 0) + base_rect.x,
				stats_t.at<int>(0, 1) + base_rect.y,
				stats_t.at<int>(0, 2), stats_t.at<int>(0, 3));
			return;
		}


		//����1��ѡ��������ģ����ܱ������ڰ�Ӱ�죬����iris���߽�ë
		//{
		//	Point maxLoc; //x�����Ȼ��0��y�����ʾindex
		//	double maxval;
		//	mymax(area, maxLoc, maxval);
		//	//img_bw = labels == maxLoc.y;
		//	int index = maxLoc.y;
		//	pupil_rect2_ = Rect(stats.at<int>(index, 0) + base_rect.x,
		//		stats.at<int>(index, 1) + base_rect.y,
		//		stats.at<int>(index, 2), stats.at<int>(index, 3));
		//}

		//����2��ѡ��������ĵ�����
		//int index = labels.at<int>(img_bw.cols/2, img_bw.rows/2);
		//if (index)
		//{
		//	pupil_rect2_ = Rect(stats.at<int>(index, 0) + base_rect.x,
		//		stats.at<int>(index, 1) + base_rect.y,
		//		stats.at<int>(index, 2), stats.at<int>(index, 3));
		//	if (pupil_rect2_.width < pupil_rect_.width/3)
		//		pupil_rect2_ = pupil_rect_; //��ֵ�õ�����̫Сʱ
		//}
		//else //�������ʱ��
		//	pupil_rect2_ = pupil_rect_;

		//����3�������������ϲ�����(���s1/s2<10)��
		//�����ʣ�����֮�����Բ��ȡ��������֪����ξ���

		//����4���������İ����ģ�û����ѡ������
		//{
		//	int index = labels.at<int>(img_bw.cols / 2, img_bw.rows / 2);
		//	if (!index || area.at<int>(index) < 0.04*img_bw.cols*img_bw.rows)
		//	{
		//		Point maxLoc; //x�����Ȼ��0��y�����ʾindex
		//		double maxval;
		//		mymax(area, maxLoc, maxval);
		//		index = maxLoc.y;
		//	}
		//	pupil_rect2_ = Rect(stats.at<int>(index, 0) + base_rect.x,
		//		stats.at<int>(index, 1) + base_rect.y,
		//		stats.at<int>(index, 2), stats.at<int>(index, 3));
		//}

		//����5�������� or �
		{
			int index = labels.at<int>(img_bw.cols / 2, img_bw.rows / 2);
			if (index && area.at<int>(index) > 0.04*img_bw.cols*img_bw.rows)
				pupil_rect_fine_ = Rect(stats.at<int>(index, 0) + base_rect.x,
					stats.at<int>(index, 1) + base_rect.y,
					stats.at<int>(index, 2), stats.at<int>(index, 3));
			else
			{
				Rect rect1 = Rect(stats_t.at<int>(0, 0), stats_t.at<int>(0, 1),
					stats_t.at<int>(0, 2), stats_t.at<int>(0, 3));
				Rect rect2 = Rect(stats_t.at<int>(1, 0), stats_t.at<int>(1, 1),
					stats_t.at<int>(1, 2), stats_t.at<int>(1, 3));
				Point centroid1(rect1.x + rect1.width / 2, rect1.y + rect1.height / 2);
				Point centroid2(rect2.x + rect2.width / 2, rect2.y + rect2.height / 2);
				int intensity1 = img.at<uchar>(centroid1);
				int intensity2 = img.at<uchar>(centroid2);
				if (abs(intensity1 - intensity2) < 5)
					pupil_rect_fine_ = rect1 | rect2;
				else
				{
					if (intensity1 > intensity2)
						pupil_rect_fine_ = rect2;
					else
						pupil_rect_fine_ = rect1;
				}
				pupil_rect_fine_ = pupil_rect_fine_ + base_rect.tl();
			}
		}
	}

	//��������(ð������)
	//area, index������������������Ϊint
	void areaSort(Mat& area, Mat& index)
	{
		index = Mat(area.rows, 1, CV_16SC1);
		for (int i = 0; i < area.rows - 1; i++)
			index.at<int>(i) = i;
		for (int i = 0; i < area.rows-1; i++)
			for (int j = i+1; j < area.rows; j++)
			{
				if (area.at<int>(i) < area.at<int>(j))
				{
					int tmp = area.at<int>(i);
					area.at<int>(i) = area.at<int>(j);
					area.at<int>(j) = tmp;
					tmp = index.at<int>(i);
					index.at<int>(i) = index.at<int>(j);
					index.at<int>(j) = tmp;
				}	
			}
	}


	void draw(Mat& img_BGR, Rect pupil_rect, Rect outer_rect, double max_response, Scalar color = RED)
	{
		//ֱ��ʹ��img��ԭʼ���ݣ����޸������ݣ�������Ҫcloneһ�ݡ�
		rectangle(img_BGR, pupil_rect, color, 1, 8);
		rectangle(img_BGR, outer_rect, color, 1, 8);
		Point center = Point(pupil_rect.x + pupil_rect.width / 2, pupil_rect.y + pupil_rect.height / 2);
		drawMarker(img_BGR, center, color);
		putNumber(img_BGR, max_response, center, color);
	}

	// overload
	void draw(Mat& img_BGR, int drawflag = 1)
	{
		draw(img_BGR, pupil_rect_coarse_, outer_rect_coarse_, max_response_coarse_, RED);
		if (drawflag)
			draw(img_BGR, pupil_rect_fine_, outer_rect_fine_, max_response_fine_, BLUE);
	}



	static void filterLight(const Mat& img_gray, Mat& img_blur, int tau)
	{
		//GaussianBlur(img_gray, img_blur, Size(5, 5), 0, 0);

		//mean shift ���Խ���Ե��խ
		//Mat temp;
		//cvtColor(img_blur, temp, CV_GRAY2BGR);
		////measureTime([&]() {bilateralFilter(img_blur, temp, 5, 100, 1, 4); }, "bilateral\t");
		//measureTime([&]() {pyrMeanShiftFiltering(temp, temp, 20, 20, 2); }, "mean shift\t");
		//cvtColor(temp, img_blur, CV_BGR2GRAY); 

		//close��������������ë��Ч���ܺ�
		//open������������С����
		//Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(7, 7));
		//Mat dst;
		//morphologyEx(img_blur, dst, MORPH_CLOSE, kernel, Point(-1, -1), 1);
		//morphologyEx(dst, dst, MORPH_OPEN, kernel, Point(-1, -1), 1);
		//Mat tmp = dst - img_gray;
		//img_blur = dst;

		//���ǿ����
		int col = img_gray.cols;
		int row = img_gray.rows;
		for (int i = 0; i < col; i++)
			for (int j = 0; j < row; j++)
			{
				if (img_gray.at<uchar>(j, i) > tau)
					img_blur.at<uchar>(j, i) = tau;
				else
					img_blur.at<uchar>(j, i) = img_gray.at<uchar>(j, i);
			}

		//close��������������ë��Ч���ܺ�
		//open������������С����
		/*Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(7, 7));
		Mat dst;
		morphologyEx(img_blur, dst, MORPH_CLOSE, kernel, Point(-1, -1), 1);
		morphologyEx(dst, dst, MORPH_OPEN, kernel, Point(-1, -1), 1);
		Mat tmp = dst - img_gray;
		img_blur = dst;*/
	}



	

private:

	// imgGray: UINT8 [0,255]. float is not allowed.
	// intergral_img: a matrix, not a image, and its values are always large.So we 
	//   set its type <int>
	//void getIntegralImg(const Mat& imgGray, Mat& integral_img);

	double getResponseMap(const Mat &integral_img, double ratio, \
		int width, int height, Rect roi, bool squareHaarFlag,
		Rect& pupil_rect, Rect& outer_rect,double& mu_inner, double& mu_outer);

	/* Computes response value with a Haar kernel.

@param inner_rect is inputoutput.
@param outer_rect.
*/
	double getResponseValue(const Mat& integral_img, Rect& inner_rect,
		Rect& outer_rect, double& mu_inner, double& mu_outer)
	{
		iterate_count_ += 1;
		// Filters rect, i.e., intersect two Rect.
		Rect boundary(0, 0, integral_img.cols - 1, integral_img.rows - 1);
		outer_rect &= boundary;
		inner_rect &= boundary;
		CV_Assert(outer_rect.width != 0);

		auto outer_bII = getBlockIntegral(integral_img, outer_rect);
		auto inner_bII = getBlockIntegral(integral_img, inner_rect);

		double f;
		mu_outer = 1.0*(outer_bII - inner_bII) / (outer_rect.area() - inner_rect.area());
		mu_inner = 1.0*inner_bII / inner_rect.area();

		f = mu_outer - kf_ * mu_inner;

		return f;
	}

	double getResponseValue2(const Mat& integral_img, \
		Rect& inner_rect, Rect& outer_rect, int responseFunc = 1)
	{
		iterate_count_ += 1;
		// Filters rect, i.e., intersect two Rect.
		Rect boundary(0, 0, integral_img.cols - 1, integral_img.rows - 1);
		outer_rect &= boundary;
		inner_rect &= boundary;
		Rect outer_rect2 = rectScale(inner_rect, 6) & boundary;
		CV_Assert(outer_rect.width != 0);

		auto outer_bII = getBlockIntegral(integral_img, outer_rect);
		auto inner_bII = getBlockIntegral(integral_img, inner_rect);
		auto outer_bII2 = getBlockIntegral(integral_img, outer_rect2);

		//�ⲿ���ڲ�rect�ľ�ֵ��. integral_image����int��double������.
		double f;
		double f1, f2;
		f1 = 1.0*(outer_bII - inner_bII) / (outer_rect.area() - \
			inner_rect.area()) - 1.0*inner_bII / inner_rect.area();
		f2 = 1.0*(outer_bII2 - inner_bII) / (outer_rect2.area() - \
			inner_rect.area()) - 1.0*inner_bII / inner_rect.area();

		if (responseFunc)
			f = f1 + f2;
		else
			f = 1.0*(outer_bII - inner_bII) / (outer_rect.area() - \
				inner_rect.area()) - 2.0*inner_bII / inner_rect.area()
			+ 10.0*log2(inner_rect.area());
		//inner_bII0 / inner_rect.area() Ч������

		return f;
	}

	/*
	@param rect must locate in the range of intergral_img.
	*/
	int getBlockIntegral(const Mat& integral_img, const Rect& rect)
	{
		/* integral_img
			  a(x1-1,y1-1)				   b (x2,y1-1)
						II(x1,y1)----------------
						|						|
						|		   Rect		  height
						|						|
			  c(x1-1,y2)---------width-------d (x2,y2)
		*/
		/* DEPRECATED
		int x1 = rect.x;
		int y1 = rect.y;
		int x2 = x1 + rect.width - 1;
		int y2 = y1 + rect.height - 1;
		int d = integral_img.at<int>(y2, x2);
		int b = y1 ? integral_img.at<int>(y1 - 1, x2) : 0;
		int c = x1 ? integral_img.at<int>(y2, x1 - 1) : 0;
		int a = x1 && y1 ? integral_img.at<int>(y1 - 1, x1 - 1) : 0;
		*/
		int d = integral_img.at<int32_t>(rect.y + rect.height, rect.x + rect.width);
		int c = integral_img.at<int32_t>(rect.y + rect.height, rect.x);
		int b = integral_img.at<int32_t>(rect.y, rect.x + rect.width);
		int a = integral_img.at<int32_t>(rect.y, rect.x);

		int integra_value = d + a - b - c;
		return integra_value;
	}


	//rectlist �洢ÿ��w�õ���Rect
	//	rectlist2 �洢�Ǽ�������֮���Rect
	// ���ǵ��ص���ʹ����������
	void rectSuppression2(vector<Rect>& rectlist, vector<double>& response,
		vector<Rect>& rectlist_out, vector<double>& response_out)
	{
		vector<Rect> rectlist2;
		vector<double> response2;
		rectSuppression(rectlist, response, rectlist2, response2);
		rectSuppression(rectlist2, response2, rectlist_out, response_out);
	}



	void rectSuppression(vector<Rect>& rectlist, vector<double>& response,
		vector<Rect>& rectlist2, vector<double>& response2)
	{
		for (int i = 0; i < rectlist.size(); ++i)
		{
			bool flag_intersect = false; //�����Ƿ�����ཻ
			for (int j = 0; j < rectlist2.size(); ++j)
			{
				Rect tmp = rectlist[i] & rectlist2[j];
				if (tmp.width) //�ཻ
				{
					flag_intersect = true;
					if (response[i] > response2[j])
					{
						rectlist2[j] = rectlist[i];
						response2[j] = response[i];
					}
					else
						continue;//break
				}

			}
			if (!flag_intersect)//��������ཻ
			{
				rectlist2.push_back(rectlist[i]);
				response2.push_back(response[i]);
			}//end j
		}//end i
	}

public:
	//--------------------input parameters--------------------
	//Haar parameters
	double ratio_outer_ = 1.42;
	double kf_ = 1.4;//��Ӧ������Ȩ�أ�inner���ֵ�
	bool useSquareHaar_ = false;//Ĭ��ʹ��horizontal Haar outer

	//optimization parameters
	int width_min_ = 31; //img.width/10
	int width_max_ = 240 / 2;//img.height/2
	int whstep_ = 4;
	int xystep_ = 4;

	//other parameters
	Size target_resolution_ = Size(320, 240);
	Rect roi_;
	
	bool useInitRect_ = false;
	Rect init_rect_ = Rect(0, 0, 0, 0); // based on original image resolution, not downsample


	//--------------------output--------------------
	Rect pupil_rect_coarse_; //coarse
	Rect outer_rect_coarse_;
	double max_response_coarse_ = -255;
	Rect pupil_rect_fine_; //refine
	Rect outer_rect_fine_;
	double max_response_fine_;

	size_t iterate_count_;

	double mu_outer_;
	double mu_inner_;
	vector<Rect> inner_rectlist_;

private:
	double ratio_downsample_;
	Rect imgboundary_;
	Rect init_rect_down;

	Mat integral_img;// size: (M+1)*(N+1)
	int inner_bII0;
};

#endif