from pathlib import Path
import matplotlib.pyplot as plt
import pprint

CYCLES_PER_SECOND = 600000000
MAX_PLOT_SECONDS = 300.0

def parse_file(filename, heap_filter=None):
    data = {}
    with open(filename, 'r') as f:
        for line in f:
            operation, timesamp, heap_id, mem_addr, size = line.strip().split(';')
            if operation not in ['ALLO', 'FREE']:
                raise ValueError(f"Invalid line: {line}")
            if (not heap_filter) or (heap_id == heap_filter):
                data[heap_id] = data.get(heap_id, []) + [(operation, int(timesamp), mem_addr, int(size))]
    return data

def plot_alloc_frees(allocs_frees):

    for key, values in allocs_frees.items():

        print(f"{len(values)} operations on heap {key}")

        y = [0]
        x = [0.0]
        # prepare x and y axis
        for operation, timesamp, _, size in values:
            if operation == 'ALLO':
                y.append(y[-1] + size if len(y) else size)
                x.append(timesamp/CYCLES_PER_SECOND)
            elif operation == 'FREE':
                y.append(y[-1] - size if len(y) else size)
                x.append(timesamp/CYCLES_PER_SECOND)

        y.append(y[-1])
        x.append(MAX_PLOT_SECONDS)

        plt.figure()
        plt.step(x, y, where='post')
        plt.xlim(left=0)
        plt.ylim(bottom=0)
        plt.xlabel('Time (s)')
        plt.ylabel('Occupation (bytes)')
        plt.title(f'Heap {key} consumption')

    plt.show()

def match_allocs_frees(allocs_frees):

    TIME_START = 0.0
    TIME_END = MAX_PLOT_SECONDS*CYCLES_PER_SECOND

    matched_operations_dict = {}
    for key, values in allocs_frees.items():

        allocations = []
        deallocations = []
        for operation, timestamp, mem_addr, size in values:
            if operation  == 'ALLO':
                allocations.append((timestamp, mem_addr, size))
            elif operation == 'FREE':
                deallocations.append((timestamp, mem_addr, size))

        matched_operations = []
        for allocation in allocations.copy():
            alloc_timestamp, alloc_mem_addr, alloc_size = allocation
            for deallocation in deallocations.copy():
                dealloc_timestamp, dealloc_mem_addr, dealloc_size = deallocation
                if ((alloc_mem_addr == dealloc_mem_addr) and
                    (alloc_timestamp <= dealloc_timestamp) and
                    (alloc_size == dealloc_size)):
                    allocations.remove(allocation)
                    deallocations.remove(deallocation)
                    matched_operations.append(
                        (alloc_mem_addr, alloc_timestamp, dealloc_timestamp, alloc_size))
                    break

        print(f"Left out allocations for heap {key}")
        pprint.pprint(allocations)
        print(f"Left out deallocations for heap {key}")
        pprint.pprint(deallocations)

        for allocation in allocations:
            alloc_timestamp, alloc_mem_addr, alloc_size = allocation
            matched_operations.append(
                (alloc_mem_addr, alloc_timestamp, TIME_END, alloc_size))
        for deallocation in deallocations:
            dealloc_timestamp, dealloc_mem_addr, dealloc_size = deallocation
            matched_operations.append(
                (dealloc_mem_addr, TIME_START, dealloc_timestamp, dealloc_size))

        matched_operations.sort(key=lambda x: x[1])
        matched_operations_dict[key] = matched_operations

    return matched_operations_dict


def plot_bar_graph(matched_operations_dict):

    for key, values in matched_operations_dict.items():

        print(f"{len(values)} alloc-free pairs on heap {key}")

        y = []
        width = []
        height = []
        left = []
        for mem_addr, alloc_timestamp, dealloc_timestamp, size in values:
            y.append((y[-1] + 1) if len(y) > 0 else 0.5)
            width.append((dealloc_timestamp - alloc_timestamp)/CYCLES_PER_SECOND)
            left.append(alloc_timestamp/CYCLES_PER_SECOND)
            height.append(1)

        plt.figure()
        plt.barh(y=y, width=width, height=height, left=left)
        plt.xlim(left=0, right=MAX_PLOT_SECONDS)
        plt.ylim(bottom=0, top=len(y))
        plt.xlabel('Time (s)')
        plt.ylabel('Memory address')
        plt.title(f'Heap {key}')

    plt.show()

if __name__ == '__main__':

    script_folder = Path(__file__).resolve().parent
    data_folder = script_folder.joinpath(".cache")
    combined_file = data_folder.joinpath('heap_operations_combined.txt')

    allocs_frees = parse_file(combined_file)
    # allocs_frees = parse_file(combined_file, heap_filter='2147486128')

    # print(f"Total allocations and deallocation operations")
    # pprint.pprint(allocs_frees)

    plot_alloc_frees(allocs_frees)

    matched_operations = match_allocs_frees(allocs_frees)

    # print(f"Matched allocation/deallocation pairs")
    # pprint.pprint(matched_operations)

    plot_bar_graph(matched_operations)
