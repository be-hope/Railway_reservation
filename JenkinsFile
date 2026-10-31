pipeline {
    agent any

    options {
        timestamps()
        timeout(time: 20, unit: 'MINUTES')
    }

    stages {

        stage('Checkout') {
            steps {
                checkout scm
            }
        }

        stage('Install Dependencies') {
            steps {
                sh '''
                    sudo apt-get update
                    sudo apt-get install -y \
                        build-essential \
                        cmake \
                        libsqlite3-dev \
                        libgtest-dev
                '''
            }
        }

        stage('Configure') {
            steps {
                sh '''
                    cmake -S . -B build \
                        -DRRS_BUILD_TESTS=ON \
                        -DRRS_WERROR=ON
                '''
            }
        }

        stage('Build') {
            steps {
                sh 'cmake --build build --parallel 2'
            }
        }

        stage('Test') {
            steps {
                sh 'ctest --test-dir build --output-on-failure'
            }
        }
    }

    post {
        success {
            echo 'Railway Reservation build and tests passed!'
        }

        failure {
            echo 'Build or tests failed. Check console output.'
        }
    }
}