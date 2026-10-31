# Railway Reservation System - CI Pipeline

## Overview

The project uses Docker to provide a consistent C++17 build and test
environment across GitHub Actions and Jenkins.

## Docker image

The root Dockerfile uses Ubuntu 24.04 and installs:

- GCC and G++
- CMake
- SQLite development libraries
- GoogleTest
- Cppcheck
- gcovr

## Pipeline stages

1. Configure the project with C++17, tests and coverage enabled.
2. Build all CMake targets.
3. Execute unit, integration and performance tests using CTest.
4. Run Cppcheck static analysis.
5. Generate XML and HTML code coverage reports using gcovr.

## Local execution

Build the Docker image:

    docker build --pull -t railway-reservation-ci .

Run the pipeline:

    mkdir -p artifacts

    docker run --rm \
      --user "$(id -u):$(id -g)" \
      -e BUILD_DIR=/tmp/rrs-build \
      -e ARTIFACTS_DIR=/workspace/artifacts \
      -v "$PWD/artifacts:/workspace/artifacts" \
      railway-reservation-ci

The pipeline can also be run directly on a machine with the required
dependencies installed:

    bash scripts/ci.sh

## GitHub Actions

The workflow is located at `.github/workflows/ci.yml`.

Pushes to all branches trigger the Docker CI job. Pull requests targeting
main or master also trigger the job.

Coverage reports are uploaded as GitHub Actions artifacts.

## Jenkins

The pipeline is defined in `Jenkinsfile`.

The Jenkins agent needs:

- Git and the Jenkins Git integration
- Docker CLI
- Access to a running Docker daemon
- GitHub plugin for the `githubPush()` trigger

Configure a GitHub repository webhook pointing to the Jenkins
`/github-webhook/` endpoint to trigger builds on pushes.

Coverage reports are archived as Jenkins build artifacts.

## Coverage outputs

The pipeline generates:

- `artifacts/coverage.xml`
- `artifacts/coverage.html`

The reports cover project source files under `src/` and `tools/`.
CI fails if configuration, compilation, tests or static analysis fails.
CI also fails if coverage report generation fails.

## Security and maintenance

Generated build files, databases and coverage reports are excluded from
the Docker build context or Git tracking where appropriate.

Dependencies are installed in the Docker image rather than directly on
Jenkins agents.
CI runs with read-only GitHub repository permissions.
CI artifacts are retained for 14 days in GitHub Actions.
CI runs use a 30-minute timeout in Jenkins.
CI runs should be reviewed for failures before merging a pull request.
