# workshop

This is my solution to an assignment for Concurrent Programming course at the University of Warsaw (MIMUW).

## Description

This is a Java program that simulates workshops with multiple workstations. Each workstation has a unique identifier and can be used by any user. Users are represented by Java threads and can enter the workshop, occupy a workstation, and switch between workstations using the Workshop interface methods provided. The program can handle multiple independent workshops with disjoint sets of workstations. Users can occupy workstations indefinitely and can enter and leave the workshop at any time. The program provides thread-safe coordination of user access to workstations.
