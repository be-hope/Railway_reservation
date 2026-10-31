pipeline {
    agent any

    triggers {
        githubPush()
    }

    options {
        timestamps()
        timeout(time: 30, unit: 'MINUTES')
    }

    stages {
        stage('Checkout') {
            steps {
                checkout scm
            }
        }

        stage('Verify Docker') {
            steps {
                sh 'docker version'
            }
        }

        stage('Build Docker Image') {
            steps {
                sh '''
                    docker build --pull \
                        --tag railway-reservation-ci:${BUILD_NUMBER} .
                '''
            }
        }

        stage('Docker Build, Tests and Analysis') {
            steps {
                sh '''
                    mkdir -p "$WORKSPACE/artifacts"

                    docker run --rm \
                        --user "$(id -u):$(id -g)" \
                        -e BUILD_DIR=/tmp/rrs-build \
                        -e ARTIFACTS_DIR=/workspace/artifacts \
                        -v "$WORKSPACE/artifacts:/workspace/artifacts" \
                        railway-reservation-ci:${BUILD_NUMBER}
                '''
            }
        }
    }

    post {
        always {
            archiveArtifacts artifacts: 'artifacts/**',
                             allowEmptyArchive: true

            sh '''
                docker image rm \
                    railway-reservation-ci:${BUILD_NUMBER} || true
            '''
        }

        success {
            echo 'Railway Reservation Docker CI passed!'
        }

        failure {
            echo 'Docker build, tests or analysis failed. Check console output.'
        }
    }
}
