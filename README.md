# Traversal Mechanics Guide

## Overview
This project implements a modular traversal system in Unreal Engine using C++.  
The mechanic is built around a custom traversal state machine that controls player behaviour across multiple movement modes.

https://mmutube.mmu.ac.uk/media/t/1_shn7jogl

The system prioritises:
- Clean and controlled state transitions  
- Separation of state logic from movement behaviour  
- Expandability for future traversal mechanics  

---

## Controls/Input

WASD to move
SPACE to jump
Hold SPACE to climb
WASD to move when climbing

## Core Traversal Flow

1. Player provides movement input  
2. Environment checks are performed (grounded, wall detected, ledge detected)  
3. Traversal state is evaluated and updated  
4. Movement and rotation behaviour are adjusted based on the active state  

All transitions are handled through a central `SetTraversalState()` function to prevent state conflicts.

---

## Traversal States

### Walking
- Default grounded movement  
- Uses standard CharacterMovementComponent behaviour  
- Serves as the base state for transitions  

### Falling
- Triggered when the character is no longer grounded  
- Uses Unreal’s built-in falling physics  
- Automatically transitions back to Walking on landing  

### Climbing
- Activated when a valid climbable surface is detected  
- Movement constrained relative to wall surface  
- Character rotation locked to face the wall  
- Previous rotation settings cached and restored on exit  

### Mantling
- Triggered when a ledge is detected within range  
- Moves the character smoothly over the obstacle  
- Returns to Walking once complete  

---

## Rotation Handling

To maintain controlled wall alignment during Climbing:

- `bUseControllerRotationYaw` is disabled  
- `bOrientRotationToMovement` is disabled  
- Character rotation is locked toward the climb surface  

When exiting Climbing:
- Cached rotation values are restored  
- Normal ground movement behaviour resumes  

This prevents unwanted turning while climbing and ensures smooth re-entry into standard movement.

---

## Technical Highlights

- Custom traversal state enum  
- Centralised state management logic  
- Surface and ledge detection using trace checks  
- Rotation-lock caching system  
- Designed to be extendable with additional traversal states  
