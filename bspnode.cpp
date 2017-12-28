// genlib - a component of the depthmapX - spatial network analysis platform
// Copyright (C) 2000-2010 University College London, Alasdair Turner
// Copyright (C) 2011-2012, Tasos Varoudis

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "bspnode.h"
#include <stack>

// Binary Space Partition

/* Takes a set of lines and creates a binary-space-partition tree by starting from a
 * root node, setting its left and right nodes and recursively doing the same process
 * over those. Through this process the set of lines is split in two (one set for each
 * left and right nodes) and those are split and passed again further down the recursion.
 * While the original implementation was actually recursive it was hitting the recursion
 * limit when the input was a large number of lines that fell on the same side (i.e. an
 * arc divided in 500 pieces). It has been refactored here to an iterative solution, where
 * the current node (left or right) is pushed to a stack along with the relevant set of lines.
 */

void BSPNode::make(Communicator *communicator, time_t atime, const std::vector<TaggedLine>& lines, BSPNode *par)
{

    typedef std::pair<std::vector<TaggedLine>, std::vector<TaggedLine> > TagLineVecPair;

    std::stack<BSPNode *> nodeStack;
    std::stack<TagLineVecPair> lineStack;

    parent = par;
    nodeStack.push(this);
    lineStack.push(makeLines(communicator, atime, lines, par));

    while(!nodeStack.empty()) {
        BSPNode *currNode = nodeStack.top();
        nodeStack.pop();
        TagLineVecPair currLines = lineStack.top();
        lineStack.pop();

        if (!currLines.first.empty()) {
           currNode->left = new BSPNode();
           nodeStack.push(currNode->left);
           lineStack.push(currNode->left->makeLines(communicator,atime,currLines.first,currNode));
        }
        if (!currLines.second.empty()) {
           currNode->right = new BSPNode();
           nodeStack.push(currNode->right);
           lineStack.push(currNode->right->makeLines(communicator,atime,currLines.second,currNode));
        }
    }
}

/* Finds the midpoint from all the lines given and returns the index of the line
 * closest to it.
 */

int BSPNode::pickMidpointLine(const std::vector<TaggedLine> &lines, BSPNode *par) {
    int chosen = -1;
    size_t i;
    Point2f midpoint;
    for (i = 0; i < lines.size(); i++) {
       midpoint += lines[i].line.start() + lines[i].line.end();
    }
    midpoint /= 2.0 * lines.size();
    bool ver = true;
    if (par && par->line.height() > par->line.width()) {
       ver = false;
    }
    double chosendist = -1.0;
    for (i = 0; i < lines.size(); i++) {
       const Line& line = lines[i].line;
       if (ver) {
          if (line.height() > line.width() && (chosen == -1 || dist(line.midpoint(),midpoint) < chosendist)) {
             chosen = i;
             chosendist = dist(line.midpoint(),midpoint);
          }
       }
       else {
          if (line.width() > line.height() && (chosen == -1 || dist(line.midpoint(),midpoint) < chosendist)) {
             chosen = i;
             chosendist = dist(line.midpoint(),midpoint);
          }
       }
    }
    // argh... and again... there weren't any hoz / ver:
    if (chosen == -1) {
       for (size_t i = 0; i < lines.size(); i++) {
          if (chosen == -1 || dist(lines[i].line.midpoint(),midpoint) < chosendist) {
             chosen = i;
             chosendist = dist(lines[i].line.midpoint(),midpoint);
          }
       }
    }
    return chosen;
}

/* Breaks a set of lines in two (left-right). First chooses a line closest to the midpoint
 * of the set ("chosen") and then classifies lines left or right depending on whether they
 * lie clockwise or anti-clockwise of the chosen one (with chosen start as centre, angles
 * from the chosen end up to 180 are clockwise, down to -180 anti-clockwise). Lines that cross
 * from one side of the chosen to the other are split in two and each part goes to the relevant set.
 */

std::pair<std::vector<TaggedLine>, std::vector<TaggedLine> > BSPNode::makeLines(Communicator *communicator,
                                                                        time_t atime,
                                                                        const std::vector<TaggedLine> &lines,
                                                                        BSPNode *par)
{
   m_count++;

   if (communicator)
   {
      if (communicator->IsCancelled()) {
         throw Communicator::CancelledException();
      }
      if (qtimer( atime, 500 )) {
         communicator->CommPostMessage( Communicator::CURRENT_RECORD, m_count );
      }
   }

   std::vector<TaggedLine> leftlines;
   std::vector<TaggedLine> rightlines;

   parent = par;

   // for optimization of the tree (this reduced a six-minute gen time to a 38 second gen time)
   int chosen = -1;
   if (lines.size() > 3) {
      chosen = pickMidpointLine(lines, par);
   }
   else {
      chosen = pafrand() % lines.size();
   }

   line = lines[chosen].line;
   m_tag = lines[chosen].tag;

   Point2f v0 = line.end() - line.start();
   v0.normalise();

   for (size_t i = 0; i < lines.size(); i++) {
      if (i == chosen) {
         continue;
      }
      const Line& testline = lines[i].line;
      int tag = lines[i].tag;
      Point2f v1 = testline.start()-line.start();
      v1.normalise();
      Point2f v2 = testline.end()-line.start();
      v2.normalise();
      // should use approxeq here:
      double a = testline.start() == line.start() ? 0 : det(v0,v1);
      double b = testline.end() == line.start() ? 0 : det(v0,v2);
      // note sure what to do if a == 0 and b == 0 (i.e., it's parallel... this test at least ensures on the line is one or the other side)
      if (a >= 0 && b >= 0) {
         leftlines.push_back(TaggedLine(testline,tag));
      }
      else if (a <= 0 && b <= 0) {
         rightlines.push_back(TaggedLine(testline,tag));
      }
      else {
         Point2f p = intersection_point(line,testline);
         Line x = Line(testline.start(),p);
         Line y = Line(p,testline.end());
         if (a >= 0) {
            if (x.length() > 0.0) // should use a tolerance here too
               leftlines.push_back(TaggedLine(x,tag));
            if (y.length() > 0.0) // should use a tolerance here too
               rightlines.push_back(TaggedLine(y,tag));
         }
         else {
            if (x.length() > 0.0) // should use a tolerance here too
               rightlines.push_back(TaggedLine(x,tag));
            if (y.length() > 0.0) // should use a tolerance here too
               leftlines.push_back(TaggedLine(y,tag));
         }
      }
   }

   return std::make_pair(leftlines, rightlines);
}

int BSPNode::classify(const Point2f& p)
{
   Point2f v0 = line.end() - line.start();
   v0.normalise();
   Point2f v1 = p - line.start();
   v1.normalise();
   if (det(v0,v1) >= 0) {
      return BSPLEFT;
   }
   else {
      return BSPRIGHT;
   }
}
