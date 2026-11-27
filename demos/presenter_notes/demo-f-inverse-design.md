# F. Inverse design: presenter notes

While the run progresses, point at the progress plot. After promoting, mention the revision record: it says which model evaluated the candidate and that it is not validated by a higher-fidelity model.

## Steps

1. **Open the acoustic focal array optimization example (a copy in the run folder: runs write checkpoints next to it)** (`open-project`)
2. **The Optimization Studio** (`workspace`)
3. **The problem: element phases as variables, the focal pressure as objective.** (`pause`)
4. **Run the optimization** (`run`)
5. **Compare the candidates** (`show`)
6. **Each row is a design that was evaluated; the best is marked.** (`pause`)
7. **Promote the best candidate into the design** (`promote`)
8. **Check the promoted design's field** (`workspace`)
9. **Field of the promoted design** (`run`)
10. **Frame the field** (`camera`)

## The story

- **problem**: Find a design that meets a goal without trying every variant by hand.
- **configure**: Design variables (the element phases), the objective (pressure at the focus) and the algorithm, saved in the project.
- **calculate**: Candidate designs, each evaluated with the acoustic model, searched by the problem's algorithm.
- **meaning**: The best candidate is the design that focuses most strongly at the target within the variables' bounds.
- **inspect**: The candidates table (objective of each candidate, the best marked), the progress plot, the promoted design revision.
- **value**: The search is recorded and repeatable; the best design is applied with one undoable command and marked unvalidated until checked.
- **assumptions**: The objective is evaluated with the analytical free-field model; the algorithm and its seed are those of the saved problem.
