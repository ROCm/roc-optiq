// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef RPV_DATAMODEL_STACK_FRAME_RECORD_H
#define RPV_DATAMODEL_STACK_FRAME_RECORD_H

#include "../common/CommonTypes.h"

// RpvDmStackFrameRecord  is a data storage class for stack trace parameters
class RpvDmStackFrameRecord 
{
    public:
        // RpvDmFlowRecord class constructor. Object stores data flow endpoint parameters
        // @param symbol - stack frame method symbol
        // @param args - stack frame method arguments
        // @param line - stack frame code line
        // @param depth - stack frame depth
        RpvDmStackFrameRecord(rocprofvis_dm_charptr_t symbol, rocprofvis_dm_charptr_t args, rocprofvis_dm_charptr_t line, uint32_t depth):
            m_symbol(symbol), m_args(args), m_code_line(line), m_depth(depth) {};
        // Returns pointer to symbol string
        rocprofvis_dm_charptr_t         Symbol() {return m_symbol.c_str();}
        // Returns pointer to argument string
        rocprofvis_dm_charptr_t         Args() {return m_args.c_str();}
        // Returns pointer to code line string
        rocprofvis_dm_charptr_t         CodeLine() {return m_code_line.c_str();}
        // returns stack frame depth
        uint32_t                        Depth() {return m_depth;}
    private:
        // stack frame symbol string
        rocprofvis_dm_string_t           m_symbol;
        // stack frame arguments string
        rocprofvis_dm_string_t           m_args;
        // stack frame code line  string
        rocprofvis_dm_string_t           m_code_line;
        // stack frame depth
        uint32_t                         m_depth;
};



#endif //RPV_DATAMODEL_STACK_FRAME_RECORD_H