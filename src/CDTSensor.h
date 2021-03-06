#pragma once

#include <sstream>
#include <iomanip>
#include <boost/filesystem.hpp>
#include "Camera.h"

namespace LinLib
{
	using namespace std;


	struct GTEntry
	{
		int id;
		int start_frame;
		int end_frame;
	};

	class GTReader
	{
		cv::Mat gt_data;

	public:
		const cv::Mat& ReadFile(string file_name)
		{
			std::ifstream stream;
			
			stream.exceptions (ifstream::failbit | ifstream::badbit);

			try { stream.open(file_name.c_str()); }
			catch (ifstream::failure) { throw new Exception("GTReader::ReadFile, Could not open the specifided file."); }

			string s;
			GTEntry entry;
			vector<GTEntry> entry_list;

			//read from file
			while (!stream.eof())
			{
				try { getline(stream, s); }
				catch (ios::failure) { break; }
				stringstream sstream(s);
				sstream.exceptions(ios::failbit);
				sstream >> entry.id >> entry.start_frame;
				try { sstream >> entry.end_frame; }
				catch (ios::failure) { entry.end_frame = -entry.start_frame; }
				entry.end_frame = -entry.end_frame;
				entry_list.push_back(entry);
			}

			stream.close();

			//pack it into a matrix
			gt_data = cv::Mat(entry_list.back().end_frame+1, 1, CV_32F, cv::Scalar(0));

			for (unsigned int i = 0; i < entry_list.size(); i++)
				for (int j = entry_list[i].start_frame; j <= entry_list[i].end_frame; j++)
					gt_data.at<float>(j) = (float)entry_list[i].id;

			return gt_data;
		}
	};

	/// Color-depth-thermal sensor interface.
	class CDTSensor
	{
	protected:
		cv::Mat color_frame, depth_frame, thermal_frame;
		bool use_color, use_depth, use_thermal;
		std::list<cv::Mat> color_buffer, depth_buffer, thermal_buffer;
		unsigned int color_delay, depth_delay, thermal_delay;
		int thermal_device;

		/// Buffer current frames - implements a frame delay for each sensor type
		void BufferFrames()
		{
			if (use_color)
			{
				if (color_delay)
				{
					if (color_buffer.size() >= color_delay)
					{
						color_buffer.front().release();
						color_buffer.pop_front();
					}
					color_buffer.push_back(color_frame.clone());
					color_frame = color_buffer.front();
				}
			}

			if (use_depth)
			{
				if (depth_delay)
				{
					if (depth_buffer.size() >= depth_delay)
					{
						depth_buffer.front().release();
						depth_buffer.pop_front();
					}
					depth_buffer.push_back(depth_frame.clone());
					depth_frame = depth_buffer.front();
				}
			}

			if (use_thermal)
			{
				if (thermal_delay)
				{
					if (thermal_buffer.size() >= thermal_delay)
					{
						thermal_buffer.front().release();
						thermal_buffer.pop_front();
					}
					thermal_buffer.push_back(thermal_frame.clone());
					thermal_frame = thermal_buffer.front();
				}
			}
		}

	public:
		CDTSensor()
			: use_color(true), use_depth(true), use_thermal(true),
			color_delay(0), depth_delay(0), thermal_delay(0), thermal_device(-1) {}

		virtual ~CDTSensor() { }

		/// Init sensor
		virtual void Init() {}

		/// Use color stream (set)
		void UseColor(bool value) { use_color = value; }
		/// Use color stream (get)
		bool UseColor() { return use_color; }
		/// Use depth stream (set)
		void UseDepth(bool value) { use_depth = value; }
		/// Use depth stream (get)
		bool UseDepth() { return use_depth; }
		/// Use thermal stream (set)
		void UseThermal(bool value) { use_thermal = value; }
		/// Use thermal stream (get)
		bool UseThermal() { return use_thermal; }

		void ColorDelay(unsigned int value) { color_delay = value; }
		unsigned int ColorDelay() { return color_delay; }
		void DepthDelay(unsigned int value) { depth_delay = value; }
		unsigned int DepthDelay() { return depth_delay; }
		void ThermalDelay(unsigned int value) { thermal_delay = value; }
		unsigned int ThermalDelay() { return thermal_delay; }

		virtual void GrabAllImages() = 0;
		cv::Mat &ColorFrame() { return color_frame; }
		cv::Mat &DepthFrame() { return depth_frame; }
		cv::Mat &ThermalFrame() { return thermal_frame; }

		void ThermalDevice(int value) { thermal_device = value; }
		int ThermalDevice() { return thermal_device; }
	};

	/// Color-depth-thermal sensor device.
	class CDTDevice : public CDTSensor
	{
		LinLib::OpenNICamera kinect_camera;
		LinLib::Camera thermal_camera;

	public:
		virtual void Init()
		{
			if (use_thermal)
				thermal_camera.Open(thermal_device);

			if (use_color || use_depth)
			{
				kinect_camera.Open();
				if (use_color)
					kinect_camera.ColorStream->Start();
				if (use_depth)
					kinect_camera.DepthStream->Start();
			}			
		}

		void SetVideoMode(SensorType type, int mode)
		{
			switch (type)
			{
			case SensorType::SENSOR_COLOR:
				kinect_camera.ColorStream->SetVideoMode(mode);
				break;
			case SensorType::SENSOR_DEPTH:
				kinect_camera.DepthStream->SetVideoMode(mode);
				break;
			case SensorType::SENSOR_IR:
				kinect_camera.IrStream->SetVideoMode(mode);
				break;
			}
		}

		virtual void GrabAllImages()
		{
			if (use_color)
				color_frame = kinect_camera.ColorStream->GetFrame();

			if (use_depth)
				depth_frame = kinect_camera.DepthStream->GetFrame();

			if (use_thermal)
				thermal_frame = thermal_camera.GetFrame();

			BufferFrames();
		}
	};

	/// Color-depth-thermal sensor file emulator.
	class CDTFile : public CDTSensor
	{
		string path;
		int reading_step, recording_step, recording_step_skip;
		int skip_frames, skip_out_of_frames;
		int start_frame, end_frame;

	public:
		CDTFile() : CDTSensor()
		{
			reading_step = 0;
			recording_step = 0;
			recording_step_skip = 0;
			skip_frames = 0;
			skip_out_of_frames = 1;
			start_frame = 0;
			end_frame = -1;
			path = "";
		}

		void SkipFrames(int value) { skip_frames = value; }
		int SkipFrames() { return skip_frames; }
		void SkipOutOfFrames(int value) { skip_out_of_frames = value; }
		int SkipOutOfFrames() { return skip_out_of_frames; }
		void StartFrame(int value) { start_frame = value; reading_step = start_frame; }
		int StartFrame() { return start_frame; }
		void EndFrame(int value) { end_frame = value; }
		int EndFrame() { return end_frame; }

		void Path(string value)
		{
			path = value;
		}

		string Path()
		{
			return path;
		}

		virtual void GrabAllImages()
		{
			if ((end_frame != -1) && (reading_step >= end_frame))
				throw new Exception("CDTFile::GrabAllImages, reached the end frame.");

			std::stringstream s;
			s << std::setw(7) << std::setfill('0') << reading_step;

			if (use_color)
			{
				color_frame = cv::imread(path + "color" + s.str() + ".png", CV_LOAD_IMAGE_GRAYSCALE); // this function is buggy on win32, release
				if (!color_frame.data)
					throw new Exception("CDTFile::GrabAllImages, could not read the specified image file.");
			}
			if (use_depth)
			{
				depth_frame = cv::imread(path + "depth" + s.str() + ".png", CV_LOAD_IMAGE_GRAYSCALE); // this function is buggy on win32, release
				if (!depth_frame.data)
					throw new Exception("CDTFile::GrabAllImages, could not read the specified image file.");
			}
			if (use_thermal)
			{
				thermal_frame = cv::imread(path + "thermal" + s.str() + ".png", CV_LOAD_IMAGE_GRAYSCALE); // this function is buggy on win32, release
				if (!thermal_frame.data)
					throw new Exception("CDTFile::GrabAllImages, could not read the specified image file.");
			}

			BufferFrames();

			reading_step++;
		}

		void SaveImages(const cv::Mat& color_image, const cv::Mat& depth_image, const cv::Mat& thermal_image)
		{
			if (!boost::filesystem::exists(boost::filesystem::path(path)))
				boost::filesystem::create_directories(boost::filesystem::path(path));

			int skip_steps = (recording_step % skip_out_of_frames);
			if (skip_steps >= skip_frames)
			{
				std::stringstream s;
				s << std::setw(7) << std::setfill('0') << recording_step_skip;
				if (color_image.data)
					cv::imwrite(path + "color" + s.str() + ".png", color_image);
				if (depth_image.data)
					cv::imwrite(path + "depth" + s.str() + ".png", depth_image);
				if (thermal_image.data)
					cv::imwrite(path + "thermal" + s.str() + ".png", thermal_image);
				recording_step_skip++;
			}
			recording_step++;
		}

		void SaveMatrix(const cv::Mat& matrix, string matrix_name)
		{
			if (!boost::filesystem::exists(boost::filesystem::path(path)))
				boost::filesystem::create_directories(boost::filesystem::path(path));

			cv::FileStorage file_storage;

			if (matrix.data)
			{
				file_storage.open(path + matrix_name + ".xml", cv::FileStorage::WRITE);
				file_storage << matrix_name << matrix;
				file_storage.release();
			}
		}

		void SaveFeatures(const cv::Mat& color_feature, const cv::Mat& depth_feature, const cv::Mat& thermal_feature)
		{
			if (!boost::filesystem::exists(boost::filesystem::path(path)))
				boost::filesystem::create_directories(boost::filesystem::path(path));

			cv::FileStorage file_storage;

			int skip_steps = (recording_step % skip_out_of_frames);
			if (skip_steps >= skip_frames)
			{
				std::stringstream s;
				s << std::setw(7) << std::setfill('0') << recording_step_skip;

				if (color_feature.data)
				{
					file_storage.open(path + "color" + s.str() + ".xml", cv::FileStorage::WRITE);
					file_storage << "color" << color_feature;
					file_storage.release();
				}

				if (depth_feature.data)
				{
					file_storage.open(path + "depth" + s.str() + ".xml", cv::FileStorage::WRITE);
					file_storage << "depth" << depth_feature;
					file_storage.release();
				}

				if (thermal_feature.data)
				{
					file_storage.open(path + "thermal" + s.str() + ".xml", cv::FileStorage::WRITE);
					file_storage << "thermal" << thermal_feature;
					file_storage.release();
				}

				recording_step_skip++;
			}
			recording_step++;
		}

		void SaveFeatureImages(const cv::Mat& color_image, const cv::Mat& depth_image, const cv::Mat& thermal_image)
		{
			if (!boost::filesystem::exists(boost::filesystem::path(path)))
				boost::filesystem::create_directories(boost::filesystem::path(path));

			int skip_steps = (recording_step % skip_out_of_frames);
			if (skip_steps >= skip_frames)
			{
				std::stringstream s;
				s << std::setw(7) << std::setfill('0') << recording_step_skip;
				if (color_image.data)
					cv::imwrite(path + "cfeature" + s.str() + ".png", color_image);
				if (depth_image.data)
					cv::imwrite(path + "dfeature" + s.str() + ".png", depth_image);
				if (thermal_image.data)
					cv::imwrite(path + "tfeature" + s.str() + ".png", thermal_image);
				recording_step_skip++;
			}
			recording_step++;
		}

	};
}