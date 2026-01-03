# Contributing to Lume

First off, thank you for considering contributing to Lume! It's people like you that make Lume such a great tool.

## How Can I Contribute?

### Reporting Bugs
- Check the [Issues](https://github.com/jorgegarcia33/lume/issues) to see if the bug has already been reported.
- If not, create a new issue. Include a clear title, a detailed description, and steps to reproduce the bug.

### Suggesting Enhancements
- Open a new issue with the tag `enhancement`.
- Describe the feature you'd like to see and why it would be useful.

### Pull Requests
1. Fork the repository.
2. Create a new branch (`git checkout -b feature/amazing-feature`).
3. Make your changes.
4. Ensure the code compiles without warnings (`make clean && make`).
5. Commit your changes (`git commit -m 'Add some amazing feature'`).
6. Push to the branch (`git push origin feature/amazing-feature`).
7. Open a Pull Request.

## Development Setup

Lume is written in C and uses `ncurses` and `pthreads`.

1. **Clone the repo**:
   ```bash
   git clone https://github.com/jorgegarcia33/lume.git
   cd lume
   ```
2. **Build**:
   ```bash
   make
   ```

## Claiming an Issue

If you find an issue you'd like to work on, simply comment:
`assign this issue to me`
Our bot will automatically assign it to you!

## Style Guide
- Use consistent indentation (4 spaces).
- Follow the existing project structure (`src/` for `.c`, `include/` for `.h`).
- Ensure all new features are documented in the README if necessary.

## License
By contributing, you agree that your contributions will be licensed under the project's existing license.
