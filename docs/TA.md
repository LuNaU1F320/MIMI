# Technical Art

Technical Art owns the visual language and asset integration standards for MIMI.

## Responsibilities

- User interface visual design
- Controller input visualization
- Unreal Engine materials
- Interaction feedback effects
- Motion graphics
- Visual style guides
- Asset naming rules
- Handoff notes for backend and Unreal teams

## Visual Direction

MIMI should feel responsive, legible, and event-ready. Visual feedback should help a facilitator and audience understand what is happening in the session without needing technical explanations.

The system should support:

- Individual user feedback
- Group energy feedback
- Clear success and failure states
- Presentation-safe visuals
- Recreation-safe high-energy visuals
- Accessible color and contrast choices

## Asset Naming Rules

Use clear prefixes and PascalCase asset names.

Recommended Unreal asset prefixes:

- `M_` for materials
- `MI_` for material instances
- `MF_` for material functions
- `NS_` for Niagara systems
- `T_` for textures
- `WBP_` for widgets
- `BP_` for Blueprints
- `DA_` for data assets
- `SM_` for static meshes
- `SK_` for skeletal meshes

Examples:

- `MI_UserPulse_Default`
- `NS_ControllerBurst_Success`
- `WBP_SessionPresencePanel`
- `DA_InputFeedbackStyle`

## Parameter Naming

Material and effect parameters should use PascalCase and describe runtime meaning:

- `UserColor`
- `InputStrength`
- `PulseRate`
- `FeedbackIntensity`
- `SessionEnergy`

## Handoff Requirements

Every TA deliverable should include:

- Asset name
- Intended runtime trigger
- Required protocol message, if applicable
- Exposed parameters
- Performance notes
- Example usage context

## First TA Milestone

1. Define a basic visual style guide.
2. Create a controller input visualization reference.
3. Define Unreal material parameter conventions.
4. Define user color assignment guidelines.
5. Coordinate with Unreal on the first input feedback implementation.

