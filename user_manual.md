# User Manual: Alarm & Output Behavior

## 1. Alarm Trigger
An alarm triggers immediately when any gauge value exceeds its **High Threshold** (Bar turns Red).

## 2. Outputs
| Output | Behavior | How to Reset |
| :--- | :--- | :--- |
| **Siren** (Audio) | Turns **ON** immediately. | Press **Acknowledge (Button 2)**. This silences the siren permanently for the current alarm event. |
| **Strobe** (Light) | Turns **ON** immediately and **Latches ON**. | Press **Reset (Button 1)**. *Note: The Strobe will only turn off if the gas level has returned to a Safe (Green) level.* |
| **Warning Screen** | Pops up immediately. | Press **Acknowledge (Button 2)**. This hides the screen for **60 seconds**. |

## 3. Warning Screen Logic
When an alarm occurs, the Warning Screen demands attention.

### Acknowledge (Snooze)
*   Pressing **Acknowledge (Button 2)** hides the Warning Screen and returns you to the Dashboard.
*   This activates a **60-Second Suppression Timer**.
*   During this 60 seconds, the screen will remain hidden to allow you to monitor the gauges.

### Re-Appearance
*   **After 60 Seconds**: If the alarm condition is **still active**, the Warning Screen will automatically pop up again to remind you.
*   The Siren will **NOT** sound again (since you already acknowledged the audio).
*   You can press Acknowledge again to "snooze" it for another 60 seconds.

### Logic Summary
> **"Visuals remind you every minute. Audio warns you once per event."**
