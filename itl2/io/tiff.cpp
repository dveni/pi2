
#include "io/itltiff.h"
#include "io/raw.h"
#include "projections.h"
#include "transform.h"

namespace itl2
{
	namespace tiff
	{
		namespace internals
		{
			string lastTiffErrorMessage;

			void tiffErrorHandler(const char *module, const char *fmt, va_list ap)
			{
				const int BUF_SIZE = 1024;
				char buf[BUF_SIZE];
				vsnprintf(buf, BUF_SIZE, fmt, ap);
				lastTiffErrorMessage = buf;
			}

			void initTIFF()
			{
				lastTiffErrorMessage = "";
				TIFFSetErrorHandler(&tiffErrorHandler);
				TIFFSetWarningHandler(&tiffErrorHandler);
			}

			std::string tiffLastError()
			{
				return lastTiffErrorMessage;
			}

			bool getCurrentDirectoryInfo(TIFF* tif, Vec3c& dimensions, ImageDataType& dataType, size_t& pixelSizeBytes, string& reason)
			{
				reason = "";

				uint16_t tiffDatatype = 0;
				uint32_t sampleFormat = 0;
				uint32_t tiffWidth = 0;
				uint32_t tiffHeight = 0;
				uint32_t tiffDepth = 0;
				uint16_t samplesPerPixel = 0;
				uint16_t bitsPerSample = 0;

				TIFFGetFieldDefaulted(tif, TIFFTAG_DATATYPE, &tiffDatatype);
				TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
				TIFFGetFieldDefaulted(tif, TIFFTAG_IMAGEWIDTH, &tiffWidth);
				TIFFGetFieldDefaulted(tif, TIFFTAG_IMAGELENGTH, &tiffHeight);
				TIFFGetFieldDefaulted(tif, TIFFTAG_IMAGEDEPTH, &tiffDepth);
				TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
				TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);

				dimensions = Vec3c(tiffWidth, tiffHeight, tiffDepth);

				switch (sampleFormat)
				{
				case SAMPLEFORMAT_UINT:
					if (bitsPerSample == 8)
					{
						dataType = ImageDataType::UInt8;
						pixelSizeBytes = 1;
					}
					else if(bitsPerSample == 16)
					{
						dataType = ImageDataType::UInt16;
						pixelSizeBytes = 2;
					}
					else if (bitsPerSample == 32)
					{
						dataType = ImageDataType::UInt32;
						pixelSizeBytes = 4;
					}
					else if (bitsPerSample == 64)
					{
						dataType = ImageDataType::UInt64;
						pixelSizeBytes = 8;
					}
					else
					{
						reason = "Unsupported unsigned integer data type.";
					}
					break;
				case SAMPLEFORMAT_INT:
					reason = "Unsupported signed integer data type.";
				case SAMPLEFORMAT_IEEEFP:
					if (bitsPerSample == 32)
					{
						dataType = ImageDataType::Float32;
						pixelSizeBytes = 4;
					}
					else
					{
						reason = "Unsupported floating point data type.";
					}
					break;
				case SAMPLEFORMAT_COMPLEXINT:
					reason = "Unsupported complex integer data type.";
				case SAMPLEFORMAT_COMPLEXIEEEFP:
					if (bitsPerSample == 64)
					{
						dataType = ImageDataType::Complex32;
						pixelSizeBytes = 8;
					}
					else
					{
						reason = "Unsupported complex floating point data type.";
					}
					break;
				case SAMPLEFORMAT_VOID:
					// Unknown data type

					// Check older tag
					switch (tiffDatatype)
					{
					case TIFF_BYTE:
						dataType = ImageDataType::UInt8;
						pixelSizeBytes = 1;
						break;
					case TIFF_SHORT:
						dataType = ImageDataType::UInt16;
						pixelSizeBytes = 2;
						break;
					case TIFF_LONG:
						dataType = ImageDataType::UInt32;
						pixelSizeBytes = 4;
						break;
					case TIFF_LONG8:
						dataType = ImageDataType::UInt64;
						pixelSizeBytes = 8;
						break;
					case TIFF_FLOAT:
						dataType = ImageDataType::Float32;
						pixelSizeBytes = 4;
						break;
					case TIFF_NOTYPE:
						dataType = ImageDataType::Unknown;
						pixelSizeBytes = bitsPerSample / 8;

						//// Guess data type (this is needed to load, e.g., tiff files generated by ImageJ)
						//if (pixelSizeBytes == 1)
						//	dataType = ImageDataType::UInt8;
						//else if (pixelSizeBytes == 2)
						//	dataType = ImageDataType::UInt16;
						//else if (pixelSizeBytes == 4)
						//	dataType = ImageDataType::Float32;
						//else if (pixelSizeBytes == 8)
						//	dataType = ImageDataType::UInt64;

						break;
					default:
						return false;
					}

					break;
				}

				if (samplesPerPixel != 1)
					reason = "Only grayscale images are supported.";

				lastTiffErrorMessage = reason;

				return reason.length() <= 0;
			}

			bool getInfo(TIFF* tif, Vec3c& dimensions, ImageDataType& dataType, size_t& pixelSizeBytes, string& reason)
			{
				// Read information from all .tif directories and make sure that all of them match.

				if (!getCurrentDirectoryInfo(tif, dimensions, dataType, pixelSizeBytes, reason))
					return false;

				coord_t dirCount = 1;

				if (TIFFLastDirectory(tif) == 0)
				{
					do
					{
						if (TIFFReadDirectory(tif) != 1)
						{
							reason = "Unable to read TIFF directory. The file invalid.";
							return false;
						}

						if (dimensions.z > 1)
						{
							reason = "TIFF file contains 3D slices.";
							return false;
						}

						Vec3c currDims;
						ImageDataType currDT;
						size_t currPixelSizeBytes;

						if (!getCurrentDirectoryInfo(tif, currDims, currDT, currPixelSizeBytes, reason))
							return false;

						if (currDims != dimensions || currDims.z > 1)
						{
							dimensions = Vec3c(0, 0, 0);
							reason = "TIFF file contains slices of different dimensions.";
							return false;
						}

						if (currDT != dataType)
						{
							reason = "TIFF file contains data of unsupported pixel data type.";
							dataType = ImageDataType::Unknown;
							currPixelSizeBytes = 0;
							return false;
						}

						if (currPixelSizeBytes != pixelSizeBytes)
						{
							reason = "TIFF file contains slices of multiple pixel data types.";
							pixelSizeBytes = 0;
							return false;
						}

						dirCount++;

					} while (TIFFLastDirectory(tif) == 0);
				}

				dimensions.z = dirCount;
				TIFFSetDirectory(tif, 0);
				return true;
			}

		}

		inline bool getInfo(const std::string& filename, Vec3c& dimensions, ImageDataType& dataType, string& reason)
		{
			internals::initTIFF();
			auto tifObj = std::unique_ptr<TIFF, decltype(TIFFClose)*>(TIFFOpen(filename.c_str(), "r"), TIFFClose);
			TIFF* tif = tifObj.get();

			if (tif)
			{
				size_t bytesPerPixel = 0;
				return internals::getInfo(tif, dimensions, dataType, bytesPerPixel, reason);
			}
			//else
			//{
			//	throw ITLException(string("Unable to open .tiff file: ") + internals::tiffLastError());
			//}

			reason = "The file does not contain a valid TIFF header.";
			return false;
		}




		namespace tests
		{
			void readWrite()
			{
				Image<uint16_t> img2;
				try
				{
					tiff::read(img2, "./input_data/t1-head_256x256x129.raw");
					throw std::runtime_error("TIFF reader did not raise exception for non-tiff file.");
				}
				catch (ITLException e)
				{
					// OK, expected exception.
				}

				Vec3c dims;
				ImageDataType dt;

				// 2D, 8-bit
				string reason;
				tiff::getInfo("./input_data/uint8.tif", dims, dt, reason);
				testAssert(reason == "", "reason");
				testAssert(dims.x == 100, "tif width");
				testAssert(dims.y == 200, "tif height");
				testAssert(dims.z == 1, "tif depth");
				testAssert(dt == ImageDataType::UInt8, "tif data type (uint8)");

				Image<uint8_t> img1;
				tiff::read(img1, "./input_data/uint8.tif");
				raw::writed(img1, "./tiff/uint8");
				tiff::writed(img1, "./tiff/uint8_out");

				Image<uint8_t> imgComp;
				tiff::read(imgComp, "./tiff/uint8_out.tif");
				testAssert(equals(img1, imgComp), "saved and loaded tiff do not equal (8-bit).");


				// 2D, 16-bit
				reason = "";
				tiff::getInfo("./input_data/uint16.tif", dims, dt, reason);
				testAssert(reason == "", "reason");
				testAssert(dims.x == 100, "tif width");
				testAssert(dims.y == 200, "tif height");
				testAssert(dims.z == 1, "tif depth");
				testAssert(dt == ImageDataType::UInt16, "tif data type (uint16)");

				
				tiff::read(img2, "./input_data/uint16.tif");
				raw::writed(img2, "./tiff/uint16");
				tiff::writed(img2, "./tiff/uint16_out");

				Image<uint16_t> imgComp2;
				tiff::read(imgComp2, "./tiff/uint16_out.tif");
				testAssert(equals(img2, imgComp2), "saved and loaded tiff do not equal (16-bit).");


				// 3d tiff files
				tiff::read(img2, "./input_data/t1-head.tif");
				Image<uint16_t> gt;
				raw::read(gt, "./input_data/t1-head");
				testAssert(equals(img2, gt), ".tif and .raw are not equal.");

				tiff::write(img2, "./tiff/t1-head.tif");
				tiff::read(imgComp2, "./tiff/t1-head.tif");
				testAssert(equals(gt, imgComp2), ".tif and .raw are not equal (3D).");

				// Tiled vs non-tiled tiff files
				Image<uint8_t> nontiled, tiled;
				tiff::read(nontiled, "./input_data/GraphicEx-cramps.tif");
				tiff::read(tiled, "./input_data/GraphicEx-cramps-tile.tif");
				testAssert(equals(nontiled, tiled), "Tiled and non-tiled .tif are not equal.");

				// Read block of head
				Image<uint16_t> headBlock(128, 128, 64);
				tiff::readBlock(headBlock, "./input_data/t1-head.tif", Vec3c(128, 128, 63), true);
				raw::writed(headBlock, "./tiff/head_block");

				Image<uint16_t> headBlockGTFull, headBlockGT(128, 128, 64);
				tiff::read(headBlockGTFull, "./input_data/t1-head.tif");
				crop(headBlockGTFull, headBlockGT, Vec3c(128, 128, 63));

				testAssert(equals(headBlock, headBlockGT), ".tif block read and crop");
			}
		}
	}
}