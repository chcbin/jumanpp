//
// Created by morita on 2017/06/12.
//

#include "numeric_creator.h"
#include <iostream>

namespace jumanpp {
namespace core {
namespace analysis {

NumericUnkMaker::NumericUnkMaker(const dic::DictionaryEntries &entries_,
                                 chars::CharacterClass charClass_,
                                 UnkNodeConfig &&info_)
    : entries_(entries_), charClass_(charClass_), info_(info_) {
  for (std::string defPattern : prefixPatternsDef) {
    CodepointStorage pattern;
    preprocessRawData(defPattern, &pattern);
    prefixPatterns.emplace_back(pattern);
  }
  for (std::string defPattern : interfixPatternsDef) {
    CodepointStorage pattern;
    preprocessRawData(defPattern, &pattern);
    interfixPatterns.emplace_back(pattern);
  }
  for (std::string defPattern : suffixPatternsDef) {
    CodepointStorage pattern;
    preprocessRawData(defPattern, &pattern);
    suffixPatterns.emplace_back(pattern);
  }
}

bool NumericUnkMaker::spawnNodes(const AnalysisInput &input,
                                 UnkNodesContext *ctx,
                                 LatticeBuilder *lattice) const {
  // Spawn the longest matting node
  auto &codepoints = input.codepoints();
  for (LatticePosition i = 0; i < codepoints.size(); ++i) {
    auto trav = entries_.traversal();
    LatticePosition nextstep = i;
    TraverseStatus status;

    auto length = FindLongestNumber(codepoints, i);  // 長さを直接返す
//    std::cerr << "legnth:" << length << " " << input.surface(i, i+length) <<  "from " << input.surface(i, codepoints.size())  << std::endl;
    if (length > 0) {
      for (; nextstep < i + length; ++nextstep) {
        status = trav.step(codepoints[nextstep].bytes);
      }

      switch (status) {
        case TraverseStatus::NoNode: {  // 同一表層のノード無し prefix でもない
          LatticePosition start = i;
          LatticePosition end = i + length;
          auto ptr = ctx->makePtr(input.surface(start, end), info_, true);
          lattice->appendSeed(ptr, start, end);
//          std::cerr << "NoNode" << "start "<< start << ", end" << end << ":" << input.surface(start, end) << std::endl;
          break;
        }
        case TraverseStatus::NoLeaf: {  // 先端
          LatticePosition start = i;
          LatticePosition end = i + length;
          auto ptr = ctx->makePtr(input.surface(start, end), info_, false);
          lattice->appendSeed(ptr, start, end);
//          std::cerr << "NoLeaf"<< "start"<< start << ", end" << end << std::endl;
          break;
        }
        case TraverseStatus::Ok:
          // オノマトペはともかく，数詞は必ずつくるのでここで作る必要がある．
          LatticePosition start = i;
          LatticePosition end = i + length;
          auto ptr = ctx->makePtr(input.surface(start, end), info_, false);
          lattice->appendSeed(ptr, start, end);
//          std::cerr << "Ok" << "start"<< start << ", end" << end << std::endl;
      }
    }
  }
  return true;
}

size_t NumericUnkMaker::checkInterfix(const CodepointStorage &codepoints,
                                      LatticePosition start,
                                      LatticePosition pos) const {
  // In other words, check conditions for fractions ("５分の１" or "数ぶんの１")
  auto restLength = codepoints.size() - (start + pos);

  if (pos > 0) {  // 先頭ではない
    for (auto itemp : interfixPatterns) {
//        std::cerr << "itemp.size() "<< itemp.size()<< std::endl;

      if (  // Interfix follows a number.
          codepoints[start + pos - 1].hasClass(charClass_) &&
          // The length of rest codepoints is longer than the pattern length
          restLength > itemp.size() &&
          // A number follows interfix.
          codepoints[start + pos + itemp.size()].hasClass(charClass_)) {
        bool matchFlag = true;
        // The pattern matches the codepoints.
        for (u16 index = 0; index < itemp.size(); ++index) {
//            std::cerr << "index" << index<<std::endl;
          if (codepoints[start + pos + index].codepoint !=
              itemp[index].codepoint) {
            matchFlag = false;
            break;
          }
        }
        if (matchFlag) return itemp.size();
      }
    }
  }
  return 0;
}

size_t NumericUnkMaker::checkSuffix(const CodepointStorage &codepoints,
                                    LatticePosition start,
                                    LatticePosition pos) const {
  // In other words, check conditions for units ("５キロ" or "数ミリ")
  auto restLength = codepoints.size() - (start + pos);

  if (pos > 0) {
    for (auto itemp : suffixPatterns) {
//        std::cerr << itemp.size() << std::endl;
      if (  // Suffix follows a number.
          codepoints[start + pos - 1].hasClass(ExceptionClass) &&
          // The pattern length does not exceed that of rest codepoints.
          restLength >= itemp.size()) {
        // The pattern matches the codepoints.
        bool matchFlag = true;
        for (u16 index = 0; index < itemp.size(); ++index) {
//            std::cerr << "index" << index << std::endl;
          if (codepoints[start + pos + index].codepoint !=
              itemp[index].codepoint) {
            matchFlag = false;
            break;
          }
        }
        if (matchFlag) return itemp.size();
      }
    }
  }
  return 0;
}

size_t NumericUnkMaker::checkPrefix(const CodepointStorage &codepoints,
                                    LatticePosition start,
                                    LatticePosition pos) const {
  // In other words, check conditions for units ("数十", "数ミリ", )

  for (auto itemp : prefixPatterns) {
    u16 suffixLength = 0;
    suffixLength = checkSuffix(codepoints, start, pos + itemp.size() );
//    std::cerr << "start" << start << ", pos "<< pos << "next:" << pos + itemp.size() << "suffixLength" << suffixLength<< std::endl;

    if (// The length of rest codepoints is longer than the pattern length
        start + pos + itemp.size() < codepoints.size() &&
        // A digit follows prefix.
        (codepoints[start + pos + itemp.size()].hasClass(DigitClass) || suffixLength >0)
          ) {
      std::cerr << "prefix condition ok" << "itemp.size()= "<< itemp.size() << "suffixLength" << suffixLength  << std::endl;
      bool matchFlag = true;
      // The pattern matches the codepoints.
      for (u16 index = 0; index < itemp.size(); ++index) {
        if (codepoints[start + pos + index].codepoint !=
            itemp[index].codepoint) {
          matchFlag = false;
          break;
        }
      }
      if (matchFlag) return itemp.size()+suffixLength;
    }
  }
  return 0;
}

size_t NumericUnkMaker::checkExceptionalCharsInFigure(
    const CodepointStorage &codepoints, LatticePosition start,
    LatticePosition pos) const {
  size_t charLength = 0;
  if ((charLength = checkPrefix(codepoints, start, pos)) > 0){
      std::cerr << "Prefix charLength" << charLength << std::endl;
      return charLength;
  }
  if ((charLength = checkInterfix(codepoints, start, pos)) > 0){
      std::cerr << "Interfix charLength" << charLength << std::endl;
    return charLength;
  }
  if ((charLength = checkSuffix(codepoints, start, pos)) > 0) {
      std::cerr << "Suffix charLength" << charLength << std::endl;
      return charLength;
  }
  if ((charLength = checkComma(codepoints, start, pos)) > 0) return charLength;
  if ((charLength = checkPeriod(codepoints, start, pos)) > 0) return charLength;
  return 0;
}

size_t NumericUnkMaker::checkComma(const CodepointStorage &codepoints,
                                   LatticePosition start,
                                   LatticePosition pos) const {
  // check exists of comma separated number beggining at pos.
  u16 numContinuedFigure = 0;
  u16 posComma = (start + pos);
  static const u16 commaInterval = 3;

  if (pos == 0) return 0;
  if (!codepoints[posComma].hasClass(CommaClass)) return 0;

  for (numContinuedFigure = 0;
       numContinuedFigure <= commaInterval + 1 &&
       posComma + 1 + numContinuedFigure < codepoints.size();
       ++numContinuedFigure) {
    if (! codepoints[posComma + 1 + numContinuedFigure].hasClass(FigureClass)) {
      break;
    }
  }
//   std::cerr << "numContinuedFigure:" << numContinuedFigure << std::endl;
  if (numContinuedFigure == 3)
    return 1;
  else
    return 0;
}

size_t NumericUnkMaker::checkPeriod(const CodepointStorage &codepoints,
                                    LatticePosition start,
                                    LatticePosition pos) const {
  // check exists of comma separated number beggining at pos.
  unsigned int posPeriod = (start + pos);

  if (pos == 0) return 0;
  if (!codepoints[posPeriod].hasClass(PeriodClass)) 
      return 0;
  if (!codepoints[posPeriod - 1].hasClass(charClass_)) 
      return 0;
  if (pos + 1 < codepoints.size() && codepoints[posPeriod + 1].hasClass(charClass_))
      return 1;
  return 0;
}

size_t NumericUnkMaker::FindLongestNumber(const CodepointStorage &codepoints,
                                          LatticePosition start) const {
  LatticePosition pos = 0;
  for (pos = 0; pos <= MaxNumericLength && start + pos < codepoints.size();
       ++pos) {
    auto &cp = codepoints[start + pos];
//    std::cerr << "pos" << pos << std::endl;
    if (!cp.hasClass(charClass_)) {
      size_t charLength = 0;

      // Check Exception
      if ((charLength = checkExceptionalCharsInFigure(codepoints, start, pos)) >
          0) {
        pos += charLength -1; // pos is inclemented by for statement
        if (! (start + pos < codepoints.size())){
            std::cerr << "over: "<< charLength <<", " << start + pos  << ", "<< codepoints.size() << std::endl;
        }
      } else {
        return pos;
      }
    }
  }

  return pos;
}

}  // namespace analysis
}  // namespace core
}  // namespace jumanpp