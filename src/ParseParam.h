#pragma once

#include <map>
#include <set>
#include <string>

namespace wnet {

class ParseParam {
  private:
    std::set<std::string> controlKeys;
    std::map<std::string, std::string> pairParams;
    std::set<std::string> singleParams;

  public:
    ParseParam(const std::set<std::string> &_controlKeys):controlKeys(_controlKeys) {}

    ~ParseParam() {}

    void parse(int argc, const char* argv[]) {
      for(int argIdx = 1; argIdx < argc; argIdx ++) {
        if(controlKeys.find(std::string(argv[argIdx])) != controlKeys.end() && argIdx != argc - 1) {
          pairParams[std::string(argv[argIdx])] = std::string(argv[argIdx + 1]);
          argIdx++;
        } else {
          singleParams.insert(std::string(argv[argIdx]));
        }
      }
    }

    std::string getPairParams(std::string param) {
      if(pairParams.find(param) != pairParams.end()) {
        return pairParams[param];
      } else {
        return "";
      }
    }

    bool checkSingleParams(std::string param) {
      if(singleParams.find(param) != singleParams.end()) {
        return true;
      } else {
        return false;
      }
    }
};

}