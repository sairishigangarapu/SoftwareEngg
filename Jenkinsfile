// UATC - Jenkins pipeline (course tooling list: Git/GitHub, Jenkins, Docker,
// GoogleTest, SonarQube).
//
// Builds in Docker for reproducibility, runs the whole test suite, records
// the results and reports pass/fail per stage.

pipeline {
    agent {
        docker {
            image 'ubuntu:24.04'
            args '-v /tmp/uatc-jenkins-cache:/cache'
        }
    }

    options {
        timestamps()
        buildDiscarder(logRotator(numToKeepStr: '30'))
        disableConcurrentBuilds()
    }

    environment {
        CC = 'gcc'
        CXX = 'g++'
    }

    stages {
        stage('Install') {
            steps {
                sh 'apt-get update && apt-get install -y --no-install-recommends build-essential cmake python3 gzip file'
            }
        }

        stage('Build') {
            steps {
                sh 'make all'
            }
        }

        stage('Test') {
            steps {
                sh 'make test'
            }
        }

        stage('SonarQube analysis') {
            steps {
                // Course tooling list: SonarQube quality gate. The scanner
                // runs as a container; if SONAR_HOST_URL/SONAR_TOKEN are
                // unset the stage is skipped rather than failing the build.
                script {
                    if (env.SONAR_HOST_URL && env.SONAR_TOKEN) {
                        sh '''
                          docker run --rm \
                            -e SONAR_HOST_URL="$SONAR_HOST_URL" \
                            -e SONAR_TOKEN="$SONAR_TOKEN" \
                            -v "$PWD:/usr/src" \
                            sonarsource/sonar-scanner-cli:latest \
                            -Dsonar.projectKey=uatc \
                            -Dsonar.sources=src,include \
                            -Dsonar.tests=tests \
                            -Dsonar.cfamily.build-wrapper-output=bw-output
                        '''
                    } else {
                        echo 'SonarQube credentials not set - skipping analysis'
                    }
                }
            }
        }

        stage('Benchmark smoke') {
            steps {
                sh '''#!/bin/bash
                      set -e
                      W=$(mktemp -d)
                      python3 -c "import random; open('$W/s.bin','wb').write(random.Random(1).randbytes(2_000_000))"
                      ./build/uatc_cli stats "$W/s.bin" | head -9
                      rm -rf "$W"
                '''
            }
        }

        stage('Demo') {
            steps {
                sh './scripts/demo.sh --clean'
            }
        }
    }

    post {
        always {
            archiveArtifacts artifacts: 'bench_results.jsonl',
                             allowEmptyArchive: true
        }
        failure {
            echo 'UATC build or tests failed'
        }
        success {
            echo 'UATC build and tests green'
        }
    }
}
