# Contributing to Replay Buffer Pro

Thank you for your interest in contributing to Replay Buffer Pro! This document provides guidelines and instructions for contributing to the project.

## Code of Conduct

By participating in this project, you agree to maintain a respectful and constructive environment for all contributors. Please:

- Use welcoming and inclusive language
- Be respectful of differing viewpoints and experiences
- Accept constructive criticism gracefully
- Focus on what's best for the community

## How to Contribute

### Reporting Bugs

1. First, check if the bug has already been reported in the Issues section
2. If not, create a new issue with the following information:
   - A clear, descriptive title
   - Detailed steps to reproduce the bug
   - Expected behavior
   - Actual behavior
   - OBS Studio version
   - Plugin version
   - Operating system and version
   - Any relevant screenshots or error messages

### Suggesting Enhancements

1. Check if the enhancement has already been suggested in the Issues section
2. If not, create a new issue with:
   - A clear, descriptive title
   - Detailed description of the proposed enhancement
   - Any potential implementation details
   - Why this enhancement would be useful to other users

### Pull Requests

1. Fork the repository
2. Create a new branch for your feature or bugfix:
   ```bash
   git checkout -b feature/your-feature-name
   # or
   git checkout -b fix/your-bugfix-name
   ```
3. Make your changes, following our coding standards
4. Test your changes thoroughly
5. Commit your changes:
   ```bash
   git commit -m "Description of your changes"
   ```
6. Push to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```
7. Open a Pull Request with:
   - A clear title and description
   - Any relevant issue numbers
   - Screenshots if applicable

## Development Setup

1. Follow the build instructions in the README.md
2. Ensure you have the following installed:
   - Visual Studio 2022 with C++ development tools
   - Qt6 development libraries
   - CMake 3.16 or newer
   - OBS Studio development files

## Coding Standards

- Use consistent indentation (spaces, not tabs)
- Follow the existing code style in the project
- Comment your code where necessary
- Keep functions focused and concise
- Use meaningful variable and function names
- Include appropriate error handling

### C++ Guidelines

- Follow modern C++ practices (C++17)
- Use smart pointers instead of raw pointers when possible
- Prefer references over pointers when appropriate
- Use const correctness
- Follow the OBS plugin API conventions

## Testing

- Test your changes with different OBS Studio versions
- Verify functionality on supported operating systems
- Check for memory leaks
- Ensure the plugin loads and unloads correctly
- Test all user interface interactions

## Documentation

When adding new features or making significant changes:

1. Update the README.md if necessary
2. Add inline documentation for new functions
3. Update any relevant user documentation
4. Include example usage if applicable

## Git Flow and Release Process

We use a branch-based flow for integrating changes and publishing releases.

### Branches
- `main`: production, tagged releases
- `develop`: integration branch for features/fixes
- `feature/*`, `fix/*`: branch from `develop`, PR back to `develop`
- `release/x.y.z`: stabilize a release from `develop`
- `hotfix/x.y.z+1`: urgent fixes branched from `main`

### Release steps
From `develop`:

```bash
git checkout develop
git pull
git checkout -b release/x.y.z
# bump version in CMakeLists.txt: project(replay-buffer-pro VERSION x.y.z)
git commit -am "chore: bump version to x.y.z"

# package the release (from your build dir)
cmake -S .. -B .
cmake --build . --config RelWithDebInfo --target create_release

# finalize release
git checkout main
git merge --no-ff release/x.y.z
git tag x.y.z
git push origin main --tags

# merge back to develop and start next iteration
git checkout develop
git merge --no-ff release/x.y.z
git push
```

Hotfixes follow the same pattern but branch from `main` as `hotfix/x.y.z+1`, bump version, tag, then merge into both `main` and `develop`.

## Questions?

If you have questions about contributing:

1. Check the existing issues and documentation
2. Create a new issue with the question if it hasn't been addressed
3. Join relevant OBS Studio development communities

## License

By contributing to Replay Buffer Pro, you agree that your contributions will be licensed under the same GPL v2 (or later) license used by the project.

---

Thank you for contributing to Replay Buffer Pro! 