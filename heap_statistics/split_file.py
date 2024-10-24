from pathlib import Path

def split_file_by_transmission(input_file, out_folder, out_prefix):

    with open(input_file, 'r') as f:
        lines = f.readlines()

        transmission_indices = [i for i, line in enumerate(lines) if 'transmission' in line]
        closing_indices = [i for i, line in enumerate(lines) if 'Edgehog device sample finished' in line]
        if not closing_indices:
            closing_indices = [i for i, line in enumerate(lines) if 'Capture done' in line]


        if len(transmission_indices) != 3:
            raise ValueError("Input file should contain exactly 3 lines with 'transmission'.")
        if len(closing_indices) != 1:
            raise ValueError("Input file should contain exactly 1 line with 'Edgehog device sample finished' or 'Capture done'.")

        # Create output files
        output_files = [out_folder.joinpath(f"{out_prefix}{i+1}.txt") for i in range(3)]

        for i, output_file in enumerate(output_files):
            start_index = transmission_indices[i] + 1
            if i < 2:
                end_index = transmission_indices[i + 1]
            else:
                end_index = closing_indices[0]

            with open(output_file, 'w') as f:
                f.writelines(lines[start_index:end_index])

if __name__ == '__main__':

    script_folder = Path(__file__).resolve().parent
    data_folder = script_folder.joinpath(".cache")

    devtty_files = sorted(f for f in data_folder.iterdir() if f.name.startswith('devtty'))

    split_file_by_transmission(devtty_files[-1], data_folder, "heap_operations")
