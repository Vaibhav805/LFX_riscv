import sys

# Quick helper to move the actual disks
def move_disk(disk, start, end):
    # Standard format, but adding a bit of logging vibe
    print(f"[ACTION] Disk {disk:2}: {start}  --->  {end}")

def solve_hanoi(n, source, target, spare):
    """
    Standard recursive approach. 
    Logic: Move the 'pile' to the spare, move the bottom disk to target, 
    then move the pile from spare to target.
    """
    # Base Case: When we're down to the last disk
    if n == 1:
        move_disk(1, source, target)
        return

    # 1. Move n-1 disks to the spare peg so we can access the bottom one
    solve_hanoi(n - 1, source, spare, target)

    # 2. Move the actual target disk to the final destination
    move_disk(n, source, target)

    # 3. Move the n-1 disks from spare back onto the target
    solve_hanoi(n - 1, spare, target, source)

if __name__ == "__main__":
    # Check for CLI args so I don't have to edit the script every time
    try:
        disks = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    except ValueError:
        print("Usage: python3 hanoi_recursion.py [number_of_disks]")
        sys.exit(1)

    print(f"Solving for {disks} disks (Expected moves: {2**disks - 1}) ")
    
    solve_hanoi(disks, 'Peg_A', 'Peg_C', 'Peg_B')
    
    print("\nRecursion complete")