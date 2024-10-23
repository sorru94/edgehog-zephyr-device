from pathlib import Path

def combine_files(file1, file2, file3, combined_file):
    common_lines = []
    with open(file1) as f1, open(file2) as f2, open(file3) as f3:
        lines2 = f2.readlines()
        lines3 = f3.readlines()
        for index, line in enumerate(f1):
            if line == lines2[index] or line == lines3[index]:
                common_lines.append(line)
            elif lines2[index] == lines3[index]:
                common_lines.append(lines2[index])
            else:
                print(line)
                print(lines2[index])
                print(lines3[index])
                raise ValueError(f"Invalid line at line: {index + 1}")

    if len(common_lines) > (65536 * 0.9):
        print("WARNING: Memory operations near the maximum number!")

    with open(combined_file, 'w') as f:
        f.writelines(common_lines)

if __name__ == '__main__':

    script_folder = Path(__file__).resolve().parent
    data_folder = script_folder.joinpath(".cache")

    file1 = data_folder.joinpath("heap_operations1.txt")
    file1 = data_folder.joinpath("heap_operations1.txt")
    file2 = data_folder.joinpath("heap_operations2.txt")
    file3 = data_folder.joinpath("heap_operations3.txt")
    combined_file = data_folder.joinpath('heap_operations_combined.txt')

    combine_files(file1, file2, file3, combined_file)
