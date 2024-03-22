// SPDX-FileCopyrightText: © 2024 vetu104
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef SNITRAY_H
#define SNITRAY_H

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus */

void start_snitray(void *client);
void SnitrayAppExit(void *client);
void SnitrayAppShow(void *client);

#ifdef __cplusplus
}
#endif /*__cplusplus */
#endif /* SNITRAY_H */
