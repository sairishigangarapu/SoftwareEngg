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
