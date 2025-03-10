'''
使用指南

Step1: 将输出信息存储到本地文件中
./test_barcode -d /path/to/image -t TYPE > output.log

Step2: 用该脚本进行过滤
python filter_results.py output.log
会生成output_true.log和output_false.log
'''

import sys
import os


def ParseLogOutput(file, file_true, file_false):
    file_true_msg = ""
    file_false_msg = ""

    tmp_msg = ""
    flag = 0
    with open(file, "r", encoding="utf-8") as f:
        for line in f.readlines():
            if "---" in line:
                if flag == 1:  # true
                    file_true_msg += tmp_msg
                else:  # false
                    file_false_msg += tmp_msg
                flag = 0
                tmp_msg = line
            else:
                tmp_msg += line
                if "<<<" in line:
                    if "success" in line:
                        flag = 1
                    elif "failure" in line:
                        flag = 0

    file_true_msg = file_true_msg[file_true_msg.find("---"):] + tmp_msg
    file_false_msg = file_false_msg[file_false_msg.find("---"):] + tmp_msg

    with open(file_true, "w") as f:
        f.write(file_true_msg)

    with open(file_false, "w") as f:
        f.write(file_false_msg)


if __name__ == '__main__':
    logfile = sys.argv[1]
    filename = os.path.splitext(logfile)[0]
    suffix = os.path.splitext(logfile)[-1]
    logfile_true = filename + "_true" + suffix
    logfile_false = filename + "_false" + suffix
    ParseLogOutput(logfile, logfile_true, logfile_false)
