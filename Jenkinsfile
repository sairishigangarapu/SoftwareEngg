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

        stage('System validation') {
            // STP system, performance and security cases on a generated
            // corpus (scripts/system_validation.sh): round trips, damaged
            // archives, path traversal, absurd sizes, single core, an
            // unreadable input as an unprivileged user, 20 repeated
            // parallel runs. One PASS/FAIL line per check in the log.
            steps {
                sh 'make cli && scripts/system_validation.sh'
            }
        }

        stage('Fuzz (ASan/UBSan)') {
            // TC-NFR-03: 20,000 random damaged archives plus an exhaustive
            // header pass, with the library built under AddressSanitizer and
            // UBSan. A failing input is kept in uatc_fuzz_test_dir/.
            steps {
                sh 'make fuzz'
            }
            post {
                failure {
                    archiveArtifacts artifacts: 'uatc_fuzz_test_dir/failure_*.uatc',
                                     allowEmptyArchive: true
                }
            }
        }

        stage('Coverage') {
            // STP exit criterion: >= 80% lines and >= 70% branches on the
            // core modules. scripts/coverage.sh fails the stage below that.
            steps {
                sh '''
                    apt-get install -y --no-install-recommends python3-venv
                    python3 -m venv .venv
                    .venv/bin/pip install -q -r tests/ui/requirements.txt
                    PATH="$PWD/.venv/bin:$PATH" scripts/coverage.sh
                '''
            }
            post {
                always {
                    archiveArtifacts artifacts: 'build-cov/coverage/**',
                                     allowEmptyArchive: true
                }
            }
        }

        stage('UI tests (Selenium)') {
            // TC-UI-01..05: drives the real page against `uatc serve` in
            // headless Chrome. Selenium Manager downloads Chrome for Testing
            // and a matching ChromeDriver; the libraries below are what
            // Chrome needs inside the Ubuntu image. UATC_UI_REQUIRED=1 makes
            // a missing browser a failure, never a silent skip.
            environment {
                UATC_UI_REQUIRED = '1'
            }
            steps {
                sh '''
                    apt-get install -y --no-install-recommends \
                        libnss3 libatk-bridge2.0-0t64 libatk1.0-0t64 libcups2t64 \
                        libdrm2 libxkbcommon0 libxcomposite1 libxdamage1 \
                        libxfixes3 libxrandr2 libgbm1 libasound2t64 \
                        libpango-1.0-0 libcairo2 fonts-liberation
                    make cli
                    .venv/bin/python -m pytest tests/ui -v \
                        --junitxml=build/ui-tests.xml
                '''
            }
            post {
                always {
                    junit testResults: 'build/ui-tests.xml',
                          allowEmptyResults: true
                }
            }
        }

        stage('SonarQube analysis') {
            steps {
                // Course tooling list: SonarQube quality gate. The scanner
                // reads sonar-project.properties from the working directory,
                // so it needs no command-line arguments. It runs as a
                // container; if SONAR_HOST_URL/SONAR_TOKEN are unset the
                // stage is skipped rather than failing the build.
                script {
                    if (env.SONAR_HOST_URL && env.SONAR_TOKEN) {
                        // The C/C++ analysis needs the compilation database.
                        sh 'cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON'
                        sh '''
                          docker run --rm \
                            -e SONAR_HOST_URL="$SONAR_HOST_URL" \
                            -e SONAR_TOKEN="$SONAR_TOKEN" \
                            -v "$PWD:/usr/src" \
                            sonarsource/sonar-scanner-cli:latest
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
