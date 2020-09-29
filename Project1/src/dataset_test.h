#pragma once

#ifndef Dataset_Test_H_
#define Dataset_Test_H_

#include <my_lib.h>
#include <opencv2/opencv.hpp>
#include "PuRe/PuRe.h"
#include "src/m_PupilDetectorHaar.h"
#include "src/pupil_extraction.h"

class DatasetTest
{
public:
	DatasetTest()
	{

	}; //构造函数


	//
	void filterImg2(const Mat& img_gray, Mat& img_blur)
	{
		GaussianBlur(img_gray, img_blur, Size(5, 5), 0, 0);

		//mean shift 可以将边缘变窄
		//Mat temp;
		//cvtColor(img_blur, temp, CV_GRAY2BGR);
		////measureTime([&]() {bilateralFilter(img_blur, temp, 5, 100, 1, 4); }, "bilateral\t");
		//measureTime([&]() {pyrMeanShiftFiltering(temp, temp, 20, 20, 2); }, "mean shift\t");
		//cvtColor(temp, img_blur, CV_BGR2GRAY); 

		//close操作可以弱化睫毛，效果很好
		//open操作可以弱化小亮斑
		Mat kernel = getStructuringElement(cv::MORPH_ELLIPSE, Size(7, 7));
		Mat dst;
		morphologyEx(img_blur, dst, cv::MORPH_CLOSE, kernel, Point(-1, -1), 1);
		morphologyEx(dst, dst, cv::MORPH_OPEN, kernel, Point(-1, -1), 1);
		Mat tmp = dst - img_gray;
		img_blur = dst;
	}



	void mydataset_init()
	{
		mydataset_dir = libpath + "data_eye/";

		{
			string filename = "imagelist.xml";
			readStringList(mydataset_dir + filename, my_imagelist);
		}

	}

	void HaarTest()
	{
		mydataset_init();

		HaarParams params;
		params.width_min = 32;
		params.width_max = 120; //240的一半
		params.wh_step = 4; //影响程序的执行速度
		params.ratio = 3;

		for (int i = 0; i < 1; ++i)//imagelist.size()
		{
			Mat img;
			std::cout << endl << my_imagelist[i] << endl;
			img = imread(mydataset_dir + my_imagelist[i]);
			Mat img_gray;
			img2Gray(img, img_gray);

			PupilDetectorHaar haar;
			haar.detect(img_gray, params);

			Mat img_haar;
			cvtColor(img_gray, img_haar, CV_GRAY2BGR);
			haar.draw(img_haar, 0);
			imshow("Eye with haar features", img_haar);
			waitKey(1000);

			string outname = "haar" + getCurrentTimeStr() + ".png";
			imwrite(outname, img_haar);
		}
	}

	void SwirskiData_init()
	{
		allDatasets_dir = "C:/Users/w9349/OneDrive - Platinum/mycodelib/pupil datasets/";

		dataset_name = "Swirski datasets";
		dataset_dir = allDatasets_dir + dataset_name + "/";
		//caselist保存每个case的文件前缀名
		readStringList2(dataset_dir + "caselist.txt", caselist);
		caselist = { "p1-left","p1-right","p2-left","p2-right" };

		results_dir = allDatasets_dir + "error/";


		//init rect的路径与导入
		ifstream fin(dataset_dir + "init_rect.txt");
		double x, y, width, height;
		while (fin >> x)
		{
			fin >> y >> width >> height;
			init_rectlist.push_back(Rect(x, y, width, height));
		}
	}

	void SwirskiTest_Haar()
	{
		SwirskiData_init();

		//method init
		string method_name = "Haar";

		HaarParams params;
		params.initRectFlag = false;
		params.squareHaarFlag = false;
		//kernel策略1：ratio多选1
		params.ratio = 1.42; //1.42, 2, 3, 4, 5, 6, 7
		//kernel策略2：mu_inner权重
		params.kf = 1; //1,1.1,1.2,1.3
		params.wh_step = 4;//2,3,4
		params.xy_step = 4;
		cout << "SwirskiTest_Haar" << "\n" << "initRectFlag=" << params.initRectFlag << "	"
			<< "squareHaarFlag=" << params.squareHaarFlag << "\n"
			<< "r=" << params.ratio << "	" << "kf=" << params.kf << "	"
			<< "wh_step=" << params.wh_step << "	" << "xy_step=" << params.xy_step << "\n";

		//for (int i = 0; i != caselist.size(); i++)//caselist.size()
		int i = 1; //单个case测试
		{
			if (params.initRectFlag)
			{
				params.init_rect = init_rectlist[i];

				//经测试，初始scale 2,3,4都可得到初始rect的响应
				params.roi = rectScale(init_rectlist[i], 2);//仅第一帧的ROI
				//可以考虑下面的方法
				//params.roi = (x, y +h/2-w/2, w, w)//init_rectlist[i];
			}


			string casename = caselist[i];
			cout << casename << endl;

			//groundtruth file. e.g., 1-1.txt
			ifstream fin_groundtruth(dataset_dir + casename + ".txt");
			{
				if (!fin_groundtruth.is_open())
					throw("cannot open file" + casename);
			}


			//保存路径
			string errorfile_name = "r" + to_string((params.ratio)) + " "
				+ dataset_name + " " + casename + ".txt";
			ofstream fout(results_dir + method_name + "/" + errorfile_name);
			{
				if (!fout.is_open())
					throw("cannot open file" + errorfile_name);
			}



			int img_index;
			char a2; //用于存储文档中的竖线
			double x, y, long_axis, short_axis, angle;
			bool firstFrameFlag = true;
			while (fin_groundtruth >> img_index)
			{
				//cout << endl << img_index << endl;
				fin_groundtruth >> a2 >> x >> y >>
					long_axis >> short_axis >> angle;
				Mat frame = imread(dataset_dir + casename + "/" + to_string(img_index) + "-eye.png");
				{
					if (frame.empty())
						throw("image import error!");
				}

				Mat img_gray;
				img2Gray(frame, img_gray);
				//filterImg(img_gray, img_gray);


				PupilDetectorHaar haar;
				haar.detect(img_gray, params);

				if (firstFrameFlag)
				{
					params.mu_inner = haar.mu_inner_;
					params.mu_outer = haar.mu_outer_;
					firstFrameFlag = false;
				}


				params.roi = Rect(0, 0, img_gray.cols, img_gray.rows);

				Mat img_haar;
				cvtColor(img_gray, img_haar, CV_GRAY2BGR);
				haar.draw(img_haar, 0);
				rectangle(img_haar, haar.pupil_rect2_, BLUE, 1, 8);

				Mat img_pupil = img_gray(haar.pupil_rect2_);




				//save results
				{
					bool flag_sucess_inner = haar.pupil_rect_.contains(Point2f(x, y));
					bool flag_sucess_outer = haar.outer_rect_.contains(Point2f(x, y));

					//flag: 是否备选的rect内含有pupil
					bool flag_sucess_candidates = false;
					vector<Rect> rectlist = haar.inner_rectlist;
					for (int i = 0; i < rectlist.size(); i++)
					{
						if (rectlist[i].contains(Point2f(x, y)))
						{
							flag_sucess_candidates = true;
							break;
						}
					}

					Point2f center(haar.pupil_rect_.x + haar.pupil_rect_.width*1.0f / 2,
						haar.pupil_rect_.y + haar.pupil_rect_.height*1.0f / 2);
					double error = norm(center - Point2f(x, y));

					bool flag_sucess_inner2 = haar.pupil_rect2_.contains(Point2f(x, y));
					Point2f center2(haar.pupil_rect2_.x + haar.pupil_rect2_.width*1.0f / 2,
						haar.pupil_rect2_.y + haar.pupil_rect2_.height*1.0f / 2);
					double error2 = norm(center2 - Point2f(x, y));




					Point2f center3;
					//利用PuRe进一步提取
					{
						Rect boundary(0, 0, img_gray.cols, img_gray.rows);
						Rect roiRect = rectScale(haar.pupil_rect2_, 1.42)&boundary;
						Mat img_t = img_gray(roiRect);
						int tau;
						//if (haar.mu_outer_ - haar.mu_inner_ > 30)
						//	tau = params.mu_outer;
						//else
						tau = haar.mu_inner_ + 30;
						PupilDetectorHaar::filterLight(img_t, img_t, tau);

						//if (haar.mu_outer_ - haar.mu_inner_ < 30)
						//	center3 = center2;
						//else
						{
							PuRe detector;
							Pupil pupil = detector.run(img_t);
							pupil.center = pupil.center + Point2f(roiRect.tl());
							if (haar.pupil_rect2_.contains(pupil.center))
							{
								drawMarker(img_haar, pupil.center, Scalar(0, 0, 255));
								if (pupil.size.width > 0)
									ellipse(img_haar, pupil, Scalar(0, 0, 255));

								center3 = pupil.center;
							}
							else
								center3 = center2;
						}
					}
					double error3 = norm(center3 - Point2f(x, y));

					RotatedRect el = RotatedRect(Point(x, y), Size(long_axis * 2, short_axis * 2), angle * 180 / PI);
					Rect2f rect = el.boundingRect2f();
					float ratio_width = haar.pupil_rect_.width*1.0f / rect.width;
					float ratio_width2 = haar.pupil_rect2_.width*1.0f / rect.width;

					fout << flag_sucess_inner << "	" << flag_sucess_outer << "	"
						<< rectlist.size() << "	" << flag_sucess_candidates << "	"
						<< error << "	" << haar.max_response_ << "	"
						<< haar.mu_inner_ << "	" << haar.mu_outer_ << "	"
						//以下是detectToFine的结果
						<< flag_sucess_inner2 << "	" << error2 << "	"
						//以下为ellipse fitting结果
						<< error3 << "	"
						<< ratio_width << "	" << ratio_width2 << endl;
				}
				//imshow("pupil region", img_pupil);
				imshow("Results", img_haar);
				waitKey(5);
			}//end while
			fin_groundtruth.close();
			fout.close();
		}//end for

	}

	void PupilnetDataset_init()
	{
		allDatasets_dir = "C:/Users/w9349/OneDrive - Platinum/mycodelib/pupil datasets/";

		dataset_name = "pupilnet datasets";
		dataset_dir = allDatasets_dir + dataset_name + "/";
		//caselist保存每个case的文件前缀名
		//readStringList2(dataset_dir + "caselist.txt", caselist);
		caselist = { "data set new I","data set new II","data set new III",
			"data set new IV","data set new V" };
		results_dir = allDatasets_dir + "error/";


		//init rect的路径与导入
		ifstream fin(dataset_dir + "init_rect.txt");
		double x, y, width, height;
		while (fin >> x)
		{
			fin >> y >> width >> height;
			init_rectlist.push_back(Rect(x, y, width, height));
		}
	}

	void PupilnetDatasetTest_Haar()
	{
		PupilnetDataset_init();

		//method init
		string method_name = "Haar";

		HaarParams params;
		params.initRectFlag = true;
		params.squareHaarFlag = true;
		//kernel策略1：ratio多选1
		params.ratio = 1.42; //1.42, 2, 3, 4, 5, 6, 7
		//kernel策略2：mu_inner权重
		params.kf = 1.4; //1,1.1,1.2,1.3
		params.wh_step = 1;//2,3,4
		params.xy_step = 4;
		cout << "PupilnetDatasetTest_Haar" << "\n" << "initRectFlag=" << params.initRectFlag << "	"
			<< "squareHaarFlag=" << params.squareHaarFlag << "\n"
			<< "r=" << params.ratio << "	" << "kf=" << params.kf << "	"
			<< "wh_step=" << params.wh_step << "	" << "xy_step=" << params.xy_step << "\n";

		for (int i = 0; i != caselist.size(); i++)//caselist.size()
		//int i = 54; //单个case测试
		{
			if (params.initRectFlag)
			{
				params.init_rect = init_rectlist[i];

				//经测试，初始scale 2,3,4都可得到初始rect的响应
				params.roi = rectScale(init_rectlist[i], 2);//仅第一帧的ROI
				//可以考虑下面的方法
				//params.roi = (x, y +h/2-w/2, w, w)//init_rectlist[i];
			}


			string casename = caselist[i];
			cout << casename << endl;

			//groundtruth file. e.g., 1-1.txt
			ifstream fin_groundtruth(dataset_dir + casename + ".txt");
			{
				if (!fin_groundtruth.is_open())
					throw("cannot open file" + casename);
			}


			//保存路径
			string errorfile_name = "r" + to_string((params.ratio)) + " "
				+ dataset_name + " " + casename + ".txt";
			ofstream fout(results_dir + method_name + "/" + errorfile_name);
			{
				if (!fout.is_open())
					throw("cannot open file" + errorfile_name);
			}


			int a2; //用于存储文档中的第一个数0
			int img_index;
			double x, y;
			bool firstFrameFlag = true;
			while (fin_groundtruth >> a2)
			{
				fin_groundtruth >> img_index >> x >> y;

				string num_s = to_string(img_index);
				string num_0;
				for (int i0 = 0; i0 < 10 - num_s.length(); i0++)
					num_0.append("0");
				num_s = num_0 + num_s;


				Mat frame = imread(dataset_dir + casename + "/" + num_s + ".png");
				{
					if (frame.empty())
						throw("image import error!");
				}

				Mat img_gray;
				img2Gray(frame, img_gray);
				//filterImg(img_gray, img_gray);


				PupilDetectorHaar haar;
				haar.detect(img_gray, params);

				if (firstFrameFlag)
				{
					params.mu_inner = haar.mu_inner_;
					params.mu_outer = haar.mu_outer_;
					firstFrameFlag = false;
				}


				params.roi = Rect(0, 0, img_gray.cols, img_gray.rows);

				Mat img_haar;
				cvtColor(img_gray, img_haar, CV_GRAY2BGR);
				haar.draw(img_haar, 0);
				rectangle(img_haar, haar.pupil_rect2_, BLUE, 1, 8);

				Mat img_pupil = img_gray(haar.pupil_rect2_);


				//save results
				{
					//groudthruth data要变换一次
					x = x / 2;
					y = img_gray.rows - y / 2;

					bool flag_sucess_inner = haar.pupil_rect_.contains(Point2f(x, y));
					bool flag_sucess_outer = haar.outer_rect_.contains(Point2f(x, y));

					//flag: 是否备选的rect内含有pupil
					bool flag_sucess_candidates = false;
					vector<Rect> rectlist = haar.inner_rectlist;
					for (int i = 0; i < rectlist.size(); i++)
					{
						if (rectlist[i].contains(Point2f(x, y)))
						{
							flag_sucess_candidates = true;
							break;
						}
					}

					Point2f center(haar.pupil_rect_.x + haar.pupil_rect_.width*1.0f / 2,
						haar.pupil_rect_.y + haar.pupil_rect_.height*1.0f / 2);
					double error = norm(center - Point2f(x, y));

					bool flag_sucess_inner2 = haar.pupil_rect2_.contains(Point2f(x, y));
					Point2f center2(haar.pupil_rect2_.x + haar.pupil_rect2_.width*1.0f / 2,
						haar.pupil_rect2_.y + haar.pupil_rect2_.height*1.0f / 2);
					double error2 = norm(center2 - Point2f(x, y));




					Point2f center3;
					//利用PuRe进一步提取
					{
						Rect boundary(0, 0, img_gray.cols, img_gray.rows);
						Rect roiRect = rectScale(haar.pupil_rect2_, 1.42)&boundary;
						Mat img_t = img_gray(roiRect);
						int tau;
						//if (haar.mu_outer_ - haar.mu_inner_ > 30)
						//	tau = params.mu_outer;
						//else
						tau = haar.mu_inner_ + 30;
						PupilDetectorHaar::filterLight(img_t, img_t, tau);

						//if (haar.mu_outer_ - haar.mu_inner_ < 30)
						//	center3 = center2;
						//else
						{
							PuRe detector;
							Pupil pupil = detector.run(img_t);
							pupil.center = pupil.center + Point2f(roiRect.tl());
							if (haar.pupil_rect2_.contains(pupil.center))
							{
								drawMarker(img_haar, pupil.center, Scalar(0, 0, 255));
								if (pupil.size.width > 0)
									ellipse(img_haar, pupil, Scalar(0, 0, 255));

								center3 = pupil.center;
							}
							else
								center3 = center2;
						}
					}
					double error3 = norm(center3 - Point2f(x, y));



					fout << flag_sucess_inner << "	" << flag_sucess_outer << "	"
						<< rectlist.size() << "	" << flag_sucess_candidates << "	"
						<< error << "	" << haar.max_response_ << "	"
						<< haar.mu_inner_ << "	" << haar.mu_outer_ << "	"
						//以下是detectToFine的结果
						<< flag_sucess_inner2 << "	" << error2 << "	"
						//以下为ellipse fitting结果
						<< error3 << "	" << endl;
				}
				//imshow("pupil region", img_pupil);
				imshow("Results", img_haar);
				waitKey(5);
			}//end while
			fin_groundtruth.close();
			fout.close();
		}//end for

	}



	void SwirskiTest()
	{
		string datasets = "C:/KernelData/0 code lib/pupil datasets/";
		string swiriski = "Swiriski datasets";
		vector<string> casenames = { "p1-left","p1-right","p2-left","p2-right" };


		for (int i = 0; i < casenames.size(); i++)
		{
			string casename = casenames[i];
			string dirname = datasets + swiriski + "/" + casename + "/";
			string filename = "pupil-ellipses.txt";
			ifstream fin(dirname + filename);
			if (!fin.is_open())
				throw("cannot open file" + casename);

			string errorname = swiriski + " " + casename + ".txt";
			ofstream fout(datasets + "error/my method/" + errorname);
			if (!fout.is_open())
				throw("cannot open file" + errorname);

			int img_index;
			char a2;
			double x, y, long_axis, short_axis, angle;
			while (fin >> img_index)
			{
				cout << endl << img_index << endl;
				fin >> a2 >> x >> y >>
					long_axis >> short_axis >> angle;
				Mat frame = imread(dirname + "frames/" + to_string(img_index) + "-eye.png");
				if (frame.empty())
					throw("image import error!");

				PupilExtractionMethod detector;
				measureTime([&]() {detector.detect(frame); }, "detector\t");
				double error = norm(detector.ellipse_rect.center - Point2f(x, y));
				fout << error << endl;
				namedWindow(casename);
				moveWindow(casename, 0, 50);
				imshow(casename, frame);
				waitKey(30);
			}
			fin.close();
			fout.close();
		}
	}

	void SwirskiTest_PuRe()
	{
		string datasets = "C:/KernelData/0 code lib/pupil datasets/";
		string swiriski = "Swiriski datasets";
		vector<string> casenames = { "p1-left","p1-right","p2-left","p2-right" };


		for (int i = 0; i < casenames.size(); i++)
		{
			string casename = casenames[i];
			string dirname = datasets + swiriski + "/" + casename + "/";
			string filename = "pupil-ellipses.txt";
			ifstream fin(dirname + filename);
			if (!fin.is_open())
				throw("cannot open file" + casename);

			string errorname = swiriski + " " + casename + ".txt";
			ofstream fout(datasets + "error/PuRe/" + errorname);
			if (!fout.is_open())
				throw("cannot open file" + errorname);

			int img_index;
			char a2;
			double x, y, long_axis, short_axis, angle;
			while (fin >> img_index)
			{
				cout << endl << img_index << endl;
				fin >> a2 >> x >> y >>
					long_axis >> short_axis >> angle;
				Mat frame = imread(dirname + "frames/" + to_string(img_index) + "-eye.png");
				if (frame.empty())
					throw("image import error!");

				PuRe detector;
				Mat frame2;
				cvtColor(frame, frame2, CV_BGR2GRAY);
				Pupil pupil = detector.run(frame2);
				drawMarker(frame, pupil.center, Scalar(0, 0, 255));
				ellipse(frame, pupil, Scalar(0, 0, 255));
				//PupilExtractionMethod detector;
				//measureTime([&]() {detector.detect(frame); }, "detector\t");
				double error = norm(pupil.center - Point2f(x, y));
				fout << error << endl;
				namedWindow(casename);
				moveWindow(casename, 0, 0);
				imshow(casename, frame);
				waitKey(30);
			}
			fin.close();
			fout.close();
		}
	}



	//LPW dataset initializataion
	void LPW_init()
	{
		allDatasets_dir = "D:/OneDrive - Platinum/mycodelib/pupil detection/";

		dataset_name = "LPW";
		dataset_dir = allDatasets_dir + dataset_name + "/";
		//caselist保存每个case的文件前缀名
		readStringList2(dataset_dir + "caselist.txt", caselist);

		results_dir = allDatasets_dir + "results/";


		//init rect的路径与导入
		//init_rect.txt是瞳孔init rect
		//init_rect2.txt是虹膜init rect
		ifstream fin(dataset_dir + "init_rect.txt");
		double x, y, width, height;
		while (fin >> x)
		{
			fin >> y >> width >> height;
			init_rectlist.push_back(Rect(x, y, width, height));
		}
	}

	void LPWTest_Haar()
	{
		LPW_init();

		//method init
		method_name = "Haar";


		//for (int i = 0; i != caselist.size(); i++)//caselist.size()
		int i = 56; //单个case测试
		//for (int i = 62; i != 64; i++)//caselist.size()
		{
			//这个最好放置在里面，否则需要每次初始化params.mu_inner，这样才不会使用上一次的数据
			HaarParams params; 
			params.initRectFlag = true;
			params.squareHaarFlag = false;
			//kernel策略1：ratio多选1
			params.ratio = 1.42; //1.42, 2, 3, 4, 5, 6, 7
			//kernel策略2：mu_inner权重
			params.kf = 1.4; //1,1.1,1.2,1.3
			params.wh_step = 2;//2,3,4
			params.xy_step = 4;
			cout << "LPWTest_Haar" << "\n" << "initRectFlag=" << params.initRectFlag << "	"
				<< "squareHaarFlag=" << params.squareHaarFlag << "\n"
				<< "r=" << params.ratio << "	" << "kf=" << params.kf << "	"
				<< "wh_step=" << params.wh_step << "	" << "xy_step=" << params.xy_step << "\n";

			if (params.initRectFlag)
			{
				params.init_rect = init_rectlist[i];

				//经测试，初始scale 2,3,4都可得到初始rect的响应
				params.roi = rectScale(init_rectlist[i], 2);//仅第一帧的ROI
				//可以考虑下面的方法
				//params.roi = (x, y +h/2-w/2, w, w)//init_rectlist[i];
			}


			string casename = caselist[i];
			cout << casename << endl;

			//groundtruth file. e.g., 1-1.txt
			ifstream fin_groundtruth(dataset_dir + casename + ".txt");
			{
				if (!fin_groundtruth.is_open())
					throw("cannot open file" + casename);
			}


			//保存路径
			string errorfile_name = "r" + to_string((params.ratio)) + " "
				+ dataset_name + " " + casename + ".txt";
			ofstream fout(results_dir + method_name + "/" + errorfile_name);
			{
				if (!fout.is_open())
					throw("cannot open file" + errorfile_name);
			}


			double x, y;
			VideoCapture cap(dataset_dir + casename + ".avi");

			PupilDetectorHaar lasthaar;
			bool firstFrameFlag = true;
			bool secondFrameFlag = false;
			int frameCount = 0;

			cv::VideoWriter coarse_vid, fine_vid;
			coarse_vid.open(to_string(i)+"coarse.avi", CV_FOURCC('M', 'J', 'P', 'G'), 95.0, cv::Size(640, 480), true);
			fine_vid.open(to_string(i) + "fine.avi", CV_FOURCC('M', 'J', 'P', 'G'), 95.0, cv::Size(640, 480), true);

			while (fin_groundtruth >> x)
			{
				frameCount += 1;
				if (frameCount == 2)
					secondFrameFlag = true;
				else
					secondFrameFlag = false;

				fin_groundtruth >> y;

				Mat frame;
				cap >> frame;
				{
					if (frame.empty())
						throw("image import error!");
				}

				Mat img_gray;
				img2Gray(frame, img_gray);
				//filterImg(img_gray, img_gray);


				PupilDetectorHaar haar;
				haar.detect(img_gray, params);

				if (firstFrameFlag)
				{
					params.mu_inner = haar.mu_inner_;
					params.mu_outer = haar.mu_outer_;
					firstFrameFlag = false;
				}


				//blink detection
				//第一帧同样适用，因为初始max_response_=-255
				//bool abnormal_flag = false;
				/*if (haar.max_response_ < lasthaar.max_response_ - 10)
				{
					haar = lasthaar;
					abnormal_flag = true;
				}
				else
					lasthaar = haar;*/

					//xy策略：ROI
					//params.roi = rectScale(haar.pupil_rect_, 4);
				params.roi = Rect(0, 0, img_gray.cols, img_gray.rows);

				Mat img_haar;
				cv::cvtColor(img_gray, img_haar, CV_GRAY2BGR);
				haar.draw(img_haar, 0);
				//coarse_vid << img_haar;
				//if (secondFrameFlag)
				//	imwrite(to_string(i)+"Coarse.png", img_haar);
				rectangle(img_haar, haar.pupil_rect2_, BLUE, 1, 8);
				//if (secondFrameFlag)
				//{
				//	imwrite(to_string(i) + "Fine.png", img_haar);
				//	break;
				//}
				//fine_vid << img_haar;
					
				bool flag_sucess_inner = haar.pupil_rect_.contains(Point2f(x, y));
				bool flag_sucess_outer = haar.outer_rect_.contains(Point2f(x, y));

				//flag: 是否备选的rect内含有pupil
				bool flag_sucess_candidates = false;
				vector<Rect> rectlist = haar.inner_rectlist;
				for (int i = 0; i < rectlist.size(); i++)
				{
					if (rectlist[i].contains(Point2f(x, y)))
					{
						flag_sucess_candidates = true;
						break;
					}
				}

				Point2f center(haar.pupil_rect_.x + haar.pupil_rect_.width*1.0f / 2,
					haar.pupil_rect_.y + haar.pupil_rect_.height*1.0f / 2);
				double error = norm(center - Point2f(x, y));

				bool flag_sucess_inner2 = haar.pupil_rect2_.contains(Point2f(x, y));
				Point2f center2(haar.pupil_rect2_.x + haar.pupil_rect2_.width*1.0f / 2,
					haar.pupil_rect2_.y + haar.pupil_rect2_.height*1.0f / 2);
				double error2 = norm(center2 - Point2f(x, y));


				//---------------------以上为Haar检测及其结果----------------------------

				Rect boundary(0, 0, img_gray.cols, img_gray.rows);
				double validRatio = 1.2; //策略：1.42
				Rect validRect = rectScale(haar.pupil_rect2_, validRatio)&boundary;
				Mat img_pupil = img_gray(validRect);
				GaussianBlur(img_pupil, img_pupil, Size(5, 5), 0, 0);


				Point2f center3;

				cv::RotatedRect ellipse_rect;
				if (haar.mu_outer_ - haar.mu_inner_ < 10) //策略：0,10,20
					center3 = center2;
				else
				{
					//edges提取
					//自适应阈值canny method
					PupilExtractionMethod detector;
					/*Mat edges;
					Rect inlinerRect = haar.pupil_rect2_ - haar.pupil_rect2_.tl();
					detector.detectPupilContour(img_pupil, edges, inlinerRect);*/

					double tau1 = 1 - 20.0 / img_pupil.cols;//策略：10，20
					Mat edges = canny_pure(img_pupil, false, false, 64 * 2, tau1, 0.5);

					Mat edges_filter;
					{
						int tau;
						//if (haar.mu_outer_ - haar.mu_inner_ > 30)
						//	tau = params.mu_outer;
						//else
						tau = haar.mu_inner_ + 100;
						//光强抑制	
						//PupilDetectorHaar::filterLight(img_t, img_t, tau);

						//1 利用光强过强抑制部分edges
						Mat img_bw;
						threshold(img_pupil, img_bw, tau, 255, cv::THRESH_BINARY);
						Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, Size(5, 5));
						dilate(img_bw, img_bw, kernel, Point(-1, -1), 1);
						edges_filter = edges & ~img_bw;

						//2 利用curves size过滤较小的片段
						edgesFilter(edges_filter);
					}
					imshow("edges", edges);
					imshow("edgesF", edges_filter);

					//int K;//iterations
					//{
					//	double p = 0.99;	// success rate 0.99
					//	double e = 0.7;		// outlier ratio, 0.7效果很好，但是时间长
					//	K = cvRound(log(1 - p) / log(1 - pow(1 - e, 5)));
					//}
					//RotatedRect ellipse_rect;
					//detector.fitPupilEllipse(edges_filter, ellipse_rect, K);


					//利用RANSAC
					detector.fitPupilEllipseSwirski(img_pupil, edges_filter, ellipse_rect);
					ellipse_rect.center = ellipse_rect.center + Point2f(validRect.tl());
					if (haar.pupil_rect2_.contains(ellipse_rect.center) && (ellipse_rect.size.width > 0))
					{
						center3 = ellipse_rect.center;
						drawMarker(img_haar, center3, Scalar(0, 0, 255));
						ellipse(img_haar, ellipse_rect, RED);
					}
					else
						center3 = center2;


					//	//利用PuRe进一步提取
					//	//PuRe detectorPuRe;
					//	//Pupil pupil = detectorPuRe.run(img_pupil);
					//	//pupil.center = pupil.center + Point2f(validRect.tl());
				}//end if


				double error3 = norm(center3 - Point2f(x, y));



				//save results
				{
					fout << flag_sucess_inner << "	" << flag_sucess_outer << "	"
						<< rectlist.size() << "	" << flag_sucess_candidates << "	"
						<< error << "	" << haar.max_response_ << "	"
						<< haar.mu_inner_ << "	" << haar.mu_outer_ << "	"
						//以下是detectToFine的结果
						<< flag_sucess_inner2 << "	" << error2 << "	"
						//以下为ellipse fitting结果
						<< error3 << endl;
				}
				//imshow("pupil region", img_pupil);
				imshow("Results", img_haar);
				waitKey(5);
			}//end while
			fin_groundtruth.close();
			fout.close();
			coarse_vid.release();
			fine_vid.release();
		}//end for

	}



	void LPWTest_PuRe()
	{
		LPW_init();

		//method init
		method_name = "PuRe";


		//for (int i = 33; i != caselist.size(); i++)//caselist.size()
		int i = 1; //单个case测试
		{
			string casename = caselist[i];
			cout << casename << endl;

			//groundtruth file. e.g., 1-1.txt
			ifstream fin_groundtruth(dataset_dir + casename + ".txt");
			{
				if (!fin_groundtruth.is_open())
					throw("cannot open file" + casename);
			}


			//保存路径
			string errorfile_name = dataset_name + " " + casename + ".txt";
			ofstream fout(results_dir + method_name + "/" + errorfile_name);
			{
				if (!fout.is_open())
					throw("cannot open file" + errorfile_name);
			}


			double x, y;
			VideoCapture cap(dataset_dir + casename + ".avi");

			while (fin_groundtruth >> x)
			{
				fin_groundtruth >> y;

				Mat frame;
				cap >> frame;
				{
					if (frame.empty())
						throw("image import error!");
				}

				Mat img_gray;
				img2Gray(frame, img_gray);

				PuRe detector;
				Pupil pupil = detector.run(img_gray);
				drawMarker(frame, pupil.center, Scalar(0, 0, 255));
				if (pupil.size.width > 0)
					ellipse(frame, pupil, Scalar(0, 0, 255));

				double error = norm(pupil.center - Point2f(x, y));

				//save results
				fout << error << endl;

				imshow("Results", frame);
				waitKey(5);
			}//end while
			fin_groundtruth.close();
			fout.close();
		}//end for
	}


	void LPW_init_iris()
	{
		allDatasets_dir = "C:/Users/w9349/OneDrive - Platinum/mycodelib/pupil datasets/";

		dataset_name = "LPW";
		dataset_dir = allDatasets_dir + dataset_name + "/";
		//caselist保存每个case的文件前缀名
		readStringList2(dataset_dir + "caselist.txt", caselist);

		results_dir = allDatasets_dir + "error/";


		//init rect的路径与导入
		//init_rect.txt是瞳孔init rect
		//init_rect2.txt是虹膜init rect
		ifstream fin(dataset_dir + "init_rect2.txt");
		double x, y, width, height;
		while (fin >> x)
		{
			fin >> y >> width >> height;
			init_rectlist.push_back(Rect(x, y, width, height/2));
		}
	}

	void LPWTest_Haar_iris()
	{
		LPW_init_iris();

		//method init
		method_name = "Haar";


		//for (int i = 0; i != caselist.size(); i++)//caselist.size()
		int i = 50; //单个case测试
		//for (int i = 62; i != 64; i++)//caselist.size()
		{
			//这个最好放置在里面，否则需要每次初始化params.mu_inner，这样才不会使用上一次的数据
			HaarParams params;
			params.initRectFlag = true;
			params.squareHaarFlag = false;
			//kernel策略1：ratio多选1
			params.ratio = 1.42; //1.42, 2, 3, 4, 5, 6, 7
			//kernel策略2：mu_inner权重
			params.kf = 2.0; //1,1.1,1.2,1.3
			params.wh_step = 2;//2,3,4
			params.xy_step = 4;
			cout << "LPWTest_Haar" << "\n" << "initRectFlag=" << params.initRectFlag << "	"
				<< "squareHaarFlag=" << params.squareHaarFlag << "\n"
				<< "r=" << params.ratio << "	" << "kf=" << params.kf << "	"
				<< "wh_step=" << params.wh_step << "	" << "xy_step=" << params.xy_step << "\n";

			if (params.initRectFlag)
			{
				params.init_rect = init_rectlist[i];

				//经测试，初始scale 2,3,4都可得到初始rect的响应
				params.roi = rectScale(init_rectlist[i], 2);//仅第一帧的ROI
				//可以考虑下面的方法
				//params.roi = (x, y +h/2-w/2, w, w)//init_rectlist[i];
			}


			string casename = caselist[i];
			cout << casename << endl;

			//groundtruth file. e.g., 1-1.txt
			ifstream fin_groundtruth(dataset_dir + casename + ".txt");
			{
				if (!fin_groundtruth.is_open())
					throw("cannot open file" + casename);
			}


			//保存路径
			string errorfile_name = "r" + to_string((params.ratio)) + " "
				+ dataset_name + " " + casename + ".txt";
			ofstream fout(results_dir + method_name + "/" + errorfile_name);
			{
				if (!fout.is_open())
					throw("cannot open file" + errorfile_name);
			}


			double x, y;
			VideoCapture cap(dataset_dir + casename + ".avi");

			PupilDetectorHaar lasthaar;
			bool firstFrameFlag = true;
			while (fin_groundtruth >> x)
			{
				fin_groundtruth >> y;

				Mat frame;
				cap >> frame;
				{
					if (frame.empty())
						throw("image import error!");
				}

				Mat img_gray;
				img2Gray(frame, img_gray);
				//filterImg(img_gray, img_gray);


				PupilDetectorHaar haar;
				haar.detectIris(img_gray, params);

				if (firstFrameFlag)
				{
					params.mu_inner = haar.mu_inner_;
					params.mu_outer = haar.mu_outer_;
					firstFrameFlag = false;
				}


				//blink detection
				//第一帧同样适用，因为初始max_response_=-255
				//bool abnormal_flag = false;
				/*if (haar.max_response_ < lasthaar.max_response_ - 10)
				{
					haar = lasthaar;
					abnormal_flag = true;
				}
				else
					lasthaar = haar;*/

					//xy策略：ROI
					//params.roi = rectScale(haar.pupil_rect_, 4);
				params.roi = Rect(0, 0, img_gray.cols, img_gray.rows);

				Mat img_haar;
				cv::cvtColor(img_gray, img_haar, CV_GRAY2BGR);
				haar.draw(img_haar, 0);
				rectangle(img_haar, haar.pupil_rect2_, BLUE, 1, 8);

				bool flag_sucess_inner = haar.pupil_rect_.contains(Point2f(x, y));
				bool flag_sucess_outer = haar.outer_rect_.contains(Point2f(x, y));

				//flag: 是否备选的rect内含有pupil
				bool flag_sucess_candidates = false;
				vector<Rect> rectlist = haar.inner_rectlist;
				for (int i = 0; i < rectlist.size(); i++)
				{
					if (rectlist[i].contains(Point2f(x, y)))
					{
						flag_sucess_candidates = true;
						break;
					}
				}

				Point2f center(haar.pupil_rect_.x + haar.pupil_rect_.width*1.0f / 2,
					haar.pupil_rect_.y + haar.pupil_rect_.height*1.0f / 2);
				double error = norm(center - Point2f(x, y));

				bool flag_sucess_inner2 = haar.pupil_rect2_.contains(Point2f(x, y));
				Point2f center2(haar.pupil_rect2_.x + haar.pupil_rect2_.width*1.0f / 2,
					haar.pupil_rect2_.y + haar.pupil_rect2_.height*1.0f / 2);
				double error2 = norm(center2 - Point2f(x, y));


				//---------------------以上为Haar检测及其结果----------------------------

				//Rect boundary(0, 0, img_gray.cols, img_gray.rows);
				//double validRatio = 1.2; //策略：1.42
				//Rect validRect = rectScale(haar.pupil_rect2_, validRatio)&boundary;
				//Mat img_pupil = img_gray(validRect);
				//GaussianBlur(img_pupil, img_pupil, Size(5, 5), 0, 0);


				Point2f center3;

				//cv::RotatedRect ellipse_rect;
				//if (haar.mu_outer_ - haar.mu_inner_ < 10) //策略：0,10,20
				//	center3 = center2;
				//else
				//{
				//	//edges提取
				//	//自适应阈值canny method
				//	PupilExtractionMethod detector;
				//	/*Mat edges;
				//	Rect inlinerRect = haar.pupil_rect2_ - haar.pupil_rect2_.tl();
				//	detector.detectPupilContour(img_pupil, edges, inlinerRect);*/

				//	double tau1 = 1 - 20.0 / img_pupil.cols;//策略：10，20
				//	Mat edges = canny_pure(img_pupil, false, false, 64 * 2, tau1, 0.5);

				//	Mat edges_filter;
				//	{
				//		int tau;
				//		//if (haar.mu_outer_ - haar.mu_inner_ > 30)
				//		//	tau = params.mu_outer;
				//		//else
				//		tau = haar.mu_inner_ + 100;
				//		//光强抑制	
				//		//PupilDetectorHaar::filterLight(img_t, img_t, tau);

				//		//1 利用光强过强抑制部分edges
				//		Mat img_bw;
				//		threshold(img_pupil, img_bw, tau, 255, cv::THRESH_BINARY);
				//		Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, Size(5, 5));
				//		dilate(img_bw, img_bw, kernel, Point(-1, -1), 1);
				//		edges_filter = edges & ~img_bw;

				//		//2 利用curves size过滤较小的片段
				//		edgesFilter(edges_filter);
				//	}
				//	imshow("edges", edges);
				//	imshow("edgesF", edges_filter);

				//	//int K;//iterations
				//	//{
				//	//	double p = 0.99;	// success rate 0.99
				//	//	double e = 0.7;		// outlier ratio, 0.7效果很好，但是时间长
				//	//	K = cvRound(log(1 - p) / log(1 - pow(1 - e, 5)));
				//	//}
				//	//RotatedRect ellipse_rect;
				//	//detector.fitPupilEllipse(edges_filter, ellipse_rect, K);


				//	//利用RANSAC
				//	detector.fitPupilEllipseSwirski(img_pupil, edges_filter, ellipse_rect);
				//	ellipse_rect.center = ellipse_rect.center + Point2f(validRect.tl());
				//	if (haar.pupil_rect2_.contains(ellipse_rect.center) && (ellipse_rect.size.width > 0))
				//	{
				//		center3 = ellipse_rect.center;
				//		drawMarker(img_haar, center3, Scalar(0, 0, 255));
				//		ellipse(img_haar, ellipse_rect, RED);
				//	}
				//	else
				//		center3 = center2;


				//	//	//利用PuRe进一步提取
				//	//	//PuRe detectorPuRe;
				//	//	//Pupil pupil = detectorPuRe.run(img_pupil);
				//	//	//pupil.center = pupil.center + Point2f(validRect.tl());
				//}//end if


				double error3 = norm(center3 - Point2f(x, y));



				//save results
				{
					fout << flag_sucess_inner << "	" << flag_sucess_outer << "	"
						<< rectlist.size() << "	" << flag_sucess_candidates << "	"
						<< error << "	" << haar.max_response_ << "	"
						<< haar.mu_inner_ << "	" << haar.mu_outer_ << "	"
						//以下是detectToFine的结果
						<< flag_sucess_inner2 << "	" << error2 << "	"
						//以下为ellipse fitting结果
						<< error3 << endl;
				}
				//imshow("pupil region", img_pupil);
				imshow("Results", img_haar);
				waitKey(5);
			}//end while
			fin_groundtruth.close();
			fout.close();
		}//end for

	}




	// my dataset
	vector<string> my_imagelist;
	string mydataset_dir;

	string allDatasets_dir;//所有数据集的路径
	string dataset_dir;//当前测试的数据集的路径
	string dataset_name;
	vector<string> caselist;
	vector<Rect> init_rectlist;

	string results_dir;

	string method_name;
private:

};





#endif