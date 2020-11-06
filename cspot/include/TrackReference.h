#ifndef TRACKREFERENCE_H
#define TRACKREFERENCE_H

#include <vector>
#include "spirc.pb.h"
#include "Utils.h"
#include <iostream>
#include <string>

class TrackReference {
private:
    std::string alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::vector<uint8_t> base62Decode(std::string uri);

public:
    TrackReference(TrackRef* ref);
    std::vector<uint8_t> gid;

    bool isEpisode = false;
    
    /**
     * @brief Returns an uri that can be allowed to query track information.
     * 
     * @return std::string
     */
    std::string getMercuryRequestUri();
};

#endif