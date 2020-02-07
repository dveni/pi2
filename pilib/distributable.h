#pragma once

#include <vector>
#include <string>

#include "argumentdatatype.h"
#include "distributor.h"

namespace pilib
{

	class PISystem;

	/**
	Base class for commands that can distribute themselves to multiple processes.
	*/
	class Distributable
	{
	public:
		/**
		Run this command in distributed manner.
		@return Output from each sub-job.
		*/
		virtual std::vector<std::string> runDistributed(Distributor& distributor, std::vector<ParamVariant>& args) const = 0;

		std::vector<std::string> runDistributed(Distributor& distributor, std::initializer_list<ParamVariant> args) const
		{
			std::vector<ParamVariant> vargs;
			vargs.insert(vargs.end(), args.begin(), args.end());
			return runDistributed(distributor, vargs);
		}

		/**
		Calculate amount of extra memory required by the command as a fraction of total size of all input and output images.
		@return extraMemFactor so that total memory needed per node or process = sum((block size) * (pixel size in bytes)) * (1 + extraMemFactor), where the sum is taken over all argument images.
		*/
		virtual double calculateExtraMemory(const std::vector<ParamVariant>& args) const
		{
			return 0.0;
		}

		/**
		This function is given coordinates of a block in reference image (first output image in argument list or first input if there are no outputs)
		and it determines the corresponding block in another argument image.
		If this method does nothing, it is assumed that the argument image can be divided similarly than the reference image.
		@param argIndex Index of argument image.
		@param readStart, readSize File position and size of data that is loaded from disk for the reference output. Relevant only for Input and InOut images.
		@param writeFilePos, writeImPos, writeSize File position, image position and size of valid data generated by the command for the given block. Relevant only for Output and InOut images. Set writeSize to all zeroes to disable writing of output file.
		*/
		virtual void getCorrespondingBlock(const std::vector<ParamVariant>& args, size_t argIndex, Vec3c& readStart, Vec3c& readSize, Vec3c& writeFilePos, Vec3c& writeImPos, Vec3c& writeSize) const
		{

		}

		/**
		Gets the execution time rating for this task.
		Returns JobType::Normal by default.
		*/
		virtual JobType getJobType(const std::vector<ParamVariant>& args) const
		{
			return JobType::Normal;
		}

		/**
		Gets preferred number of subdivisions in the first distribution direction for this command.
		By default returns 1.
		*/
		virtual size_t getPreferredSubdivisions(const std::vector<ParamVariant>& args) const
		{
			return 1;
		}

		/**
		Gets first distribution direction allowed for this command.
		By default z.
		*/
		virtual size_t getDistributionDirection1(const std::vector<ParamVariant>& args) const
		{
			return 2;
		}

		/**
		Gets second distribution direction allowed for this command.
		By default none.
		*/
		virtual size_t getDistributionDirection2(const std::vector<ParamVariant>& args) const
		{
			return std::numeric_limits<size_t>::max();
		}

		/**
		Gets amount of overlap required between blocks processed at different nodes.
		Default value is zero.
		The value must be given in reference image coordinates (relevant if the command changes image size).
		*/
		virtual Vec3c getMargin(const std::vector<ParamVariant>& args) const
		{
			return Vec3c(0, 0, 0);
		}

		/**
		Returns index of argument image (in argument list) that is used to calculate block sizes.
		Default value corresponds to first output image or first input image if there are no outputs.
		*/
		virtual size_t getRefIndex(const std::vector<ParamVariant>& args) const
		{
			return std::numeric_limits<size_t>::max();
		}

		/**
		Returns a value indicating whether the distributed processing of the current command can be combined
		with other commands without separate read-write whole image cycle.
		By default false.
		Conditions that must be fulfilled by commands that return true:
		- The command must be able to process data with any margin greater than or equal to the margin it indicates.
		- The command does not produce output vector<string>, or the output can be discarded.
		*/
		virtual bool canDelay(const std::vector<ParamVariant>& args) const
		{
			return false;
		}
	};
}
