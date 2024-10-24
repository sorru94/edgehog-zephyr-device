from pathlib import Path

def clean_file(input_file, out_file):

    strings_to_remove = [
        'Called stream aggregated function when the device is not connected',
        'Unable to send system_status'
    ]

    with open(input_file, 'r') as f_in, open(out_file, 'w') as f_out:
        for line in f_in:
            if (strings_to_remove[0] not in line) and (strings_to_remove[1] not in line):
                f_out.write(line)



if __name__ == '__main__':

    script_folder = Path(__file__).resolve().parent
    data_folder = script_folder.joinpath(".cache")

    devtty_files = sorted(f for f in data_folder.iterdir() if f.name.startswith('devtty'))

    clean_file(devtty_files[-1], devtty_files[-1].with_name(devtty_files[-1].stem + '.copy' + devtty_files[-1].suffix))
