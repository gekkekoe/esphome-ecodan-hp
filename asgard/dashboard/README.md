# Asgard Dashboard Installation Guide

This guide explains how to add the dashboard using the Home Assistant interface.

## 1. Get the Code
1.  Open the file `asgard-dashboard.yaml`.
2.  Copy the **entire content** of this file to your clipboard.

## 2. Add to Home Assistant

1.  Open your Home Assistant dashboard.
2.  Enter edit mode (usually by clicking the pencil icon in the top right corner).
3.  Click the **Plus (+)** button in the top navigation bar to add a new view.
4.  **Configure the view:**
    * **Title:** Enter a name (e.g., Heat Pump).
    * **View Type:** Select **Sections**.
5.  **Switch to code editor:**
    * Do **not** click Save yet.
    * Click the 3 dots > "Edit in YAML".
6.  **Paste the code:**
    * Delete the default code already in the window.
    * **Paste** the content of `asgard-dashboard.yaml`.
7.  Click **Save**.
