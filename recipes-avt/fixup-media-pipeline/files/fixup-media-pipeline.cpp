#include <iostream>
#include <vector>
#include <filesystem>
#include <sstream>
#include <set>
#include <algorithm>
#include <regex>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>

namespace fs = std::filesystem;

const std::set<uint32_t> ignoreMBusCodes = {
     0x5002,
};

std::string getEntityPath(int mediaFd,const media_entity_desc &entityDesc)
{
    if (ioctl(mediaFd,MEDIA_IOC_ENUM_ENTITIES,&entityDesc) != 0)
        return "";

    std::stringstream charDevId{};
    charDevId << entityDesc.dev.major << ":" << entityDesc.dev.minor;

    auto const charDevPath = fs::path{"/dev/char/"} / charDevId.str();
    auto const devPath = fs::canonical(charDevPath);

    return devPath;
}

void readEntityLinks(const int mediaFd,uint32_t entityId) {

    media_entity_desc entityDesc{.id = entityId};

    if (ioctl(mediaFd,MEDIA_IOC_ENUM_ENTITIES,&entityDesc) != 0)
        return;

    media_links_enum linksEnum{};
    linksEnum.links = new media_link_desc[entityDesc.links];
    linksEnum.entity = entityId;

    auto const sourcePath = getEntityPath(mediaFd,entityDesc);

    auto sourceFd = open(sourcePath.c_str(),O_RDWR);

    if (ioctl(mediaFd,MEDIA_IOC_ENUM_LINKS,&linksEnum) == 0)
    {
        for (auto i = 0; i < entityDesc.links; i++)
        {
            auto const & link = linksEnum.links[i];
            auto const sinkEntityId = link.sink.entity;

            std::cout << "Source: " << link.source.entity << " pad: " << link.source.index << std::endl;
            std::cout << "Sink: " << sinkEntityId << " pad: " << link.sink.index << std::endl;

            media_entity_desc sinkEntityDesc{.id = sinkEntityId};

            if (ioctl(mediaFd,MEDIA_IOC_ENUM_ENTITIES,&entityDesc) != 0)
                continue;

            auto const sinkPath = getEntityPath(mediaFd,sinkEntityDesc);

            auto sinkFd = open(sinkPath.c_str(),O_RDWR);

            if (entityDesc.type & MEDIA_ENT_F_OLD_SUBDEV_BASE && sinkEntityDesc.type & MEDIA_ENT_F_OLD_SUBDEV_BASE) {
                std::set<uint32_t> sourceMBusCodes{};

                v4l2_subdev_mbus_code_enum sourceMBusCodeEnum{
                        .pad = link.source.index,
                        .index = 0,
                        .which = V4L2_SUBDEV_FORMAT_TRY,
                };
                while (ioctl(sourceFd, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &sourceMBusCodeEnum) == 0) {
                    sourceMBusCodes.insert(sourceMBusCodeEnum.code);

                    sourceMBusCodeEnum.index++;
                }


                std::set<uint32_t> sinkMBusCodes{};

                v4l2_subdev_mbus_code_enum sinkMBusCodeEnum{
                        .pad = link.sink.index,
                        .index = 0,
                        .which = V4L2_SUBDEV_FORMAT_TRY,
                };
                while (ioctl(sinkFd, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &sinkMBusCodeEnum) == 0) {
                    sinkMBusCodes.insert(sinkMBusCodeEnum.code);

                    sinkMBusCodeEnum.index++;
                }


                std::vector<uint32_t> commonMBusCodes{};
                std::set_intersection(sourceMBusCodes.begin(), sourceMBusCodes.end(), sinkMBusCodes.begin(),
                                      sinkMBusCodes.end(), std::back_inserter(commonMBusCodes));

                uint32_t selectedMBusCode = 0;

                for (auto const mbusCode: commonMBusCodes) {
                    if (ignoreMBusCodes.count(mbusCode) == 0) {
                        selectedMBusCode = mbusCode;
                        break;
                    }
                }

                if (selectedMBusCode) {
                    v4l2_subdev_format sourceFmt{
                            .which = V4L2_SUBDEV_FORMAT_ACTIVE,
                            .pad = link.source.index,
                    };

                    if (ioctl(sourceFd, VIDIOC_SUBDEV_G_FMT, &sourceFmt) != 0)
                        continue;

                    sourceFmt.format.code = selectedMBusCode;

                    if (ioctl(sourceFd, VIDIOC_SUBDEV_S_FMT, &sourceFmt) != 0)
                        continue;

                    v4l2_subdev_format sinkFmt = sourceFmt;
                    sinkFmt.pad = link.sink.index;

                    if (ioctl(sinkFd, VIDIOC_SUBDEV_S_FMT, &sinkFmt) != 0)
                        continue;
                }
            }
            else if (entityDesc.type & MEDIA_ENT_F_OLD_SUBDEV_BASE && sinkEntityDesc.type == MEDIA_ENT_F_IO_V4L) {
                v4l2_subdev_format sourceFmt{
                        .which = V4L2_SUBDEV_FORMAT_ACTIVE,
                        .pad = link.source.index,
                };

                if (ioctl(sourceFd, VIDIOC_SUBDEV_G_FMT, &sourceFmt) != 0)
                    continue;

                v4l2_capability capability{};

                if (ioctl(sinkFd, VIDIOC_QUERYCAP, &capability) != 0)
                    continue;



                if (capability.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
                {
                    v4l2_fmtdesc fmtdesc{
                        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                    };

                    if (ioctl(sinkFd,VIDIOC_ENUM_FMT,&fmtdesc) != 0)
                        continue;


                    v4l2_format format{
                        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                    };

                    if (ioctl(sinkFd, VIDIOC_G_FMT, &format) != 0)
                        continue;

                    format.fmt.pix_mp.width = sourceFmt.format.width;
                    format.fmt.pix_mp.height = sourceFmt.format.height;
                    format.fmt.pix_mp.pixelformat = fmtdesc.pixelformat;

                    if (ioctl(sinkFd, VIDIOC_S_FMT, &format) != 0)
                        continue;
                }
                else if (capability.device_caps & V4L2_CAP_VIDEO_CAPTURE)
                {
                    v4l2_fmtdesc fmtdesc{
                            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                    };

                    if (ioctl(sinkFd,VIDIOC_ENUM_FMT,&fmtdesc) != 0)
                        continue;

                    v4l2_format format{
                            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                    };

                    if (ioctl(sinkFd, VIDIOC_G_FMT, &format) != 0)
                        continue;

                    format.fmt.pix.width = sourceFmt.format.width;
                    format.fmt.pix.height = sourceFmt.format.height;
                    format.fmt.pix.pixelformat = fmtdesc.pixelformat;

                    if (ioctl(sinkFd, VIDIOC_S_FMT, &format) != 0)
                        continue;
                }
            }

            readEntityLinks(mediaFd,sinkEntityId);
        }
    }
};

int main() {

  fs::path const mediaControllerBasePath{"/dev"};
  std::regex const mediaControllerRegex{"media[0-9]+"};
  std::vector<fs::path> mediaControllers;

  for (const auto & entry : fs::directory_iterator{mediaControllerBasePath})
  {
    if (entry.is_character_file() && std::regex_match(entry.path().filename().string(),mediaControllerRegex))
    {
        auto const mediaFd = open(entry.path().c_str(),O_RDWR);
    
        if (mediaFd < 0)
            continue;
    
        std::vector<uint32_t> sensorEntities{};
    
        media_entity_desc entityDesc{.id = __u32(MEDIA_ENT_ID_FLAG_NEXT)};
    
        while (ioctl(mediaFd,MEDIA_IOC_ENUM_ENTITIES,&entityDesc) == 0)
        {
            if (entityDesc.type == MEDIA_ENT_T_V4L2_SUBDEV_SENSOR)
                sensorEntities.push_back(entityDesc.id);
    
            entityDesc.id |= MEDIA_ENT_ID_FLAG_NEXT;
        }
    
    
        for (auto const sensorEntityId : sensorEntities)
        {
            std::cout << sensorEntityId << std::endl;
            readEntityLinks(mediaFd,sensorEntityId);
        }
        
        close(mediaFd);
    }
  }
  
    

    return 0;
}

/*
 *


 */