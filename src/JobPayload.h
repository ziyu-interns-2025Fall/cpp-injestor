// src/JobPayload.h
#pragma once
#include <string>

struct JobPayload {
    std::string job_id;
    std::string pdf_path;
    std::string config_type;
    std::string target_collection;
};
