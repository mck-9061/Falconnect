plugins {
    id("java")
    id("org.springframework.boot") version "4.1.0"
}

group = "net.falconnect"
version = "1.0-SNAPSHOT"

apply { plugin("io.spring.dependency-management") }

repositories {
    mavenCentral()
}

dependencies {
    testImplementation(platform("org.junit:junit-bom:5.10.0"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

tasks.test {
    useJUnitPlatform()
}

