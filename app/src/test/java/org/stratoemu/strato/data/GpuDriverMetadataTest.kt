/* SPDX-License-Identifier: MPL-2.0 */
package org.stratoemu.strato.data

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GpuDriverMetadataTest {
    private fun metadata(name: String, packageVersion: String) = GpuDriverMetadata(
        name = name,
        author = "",
        packageVersion = packageVersion,
        vendor = "",
        driverVersion = "",
        minApi = 0,
        description = "",
        libraryName = "",
    )

    @Test
    fun identifiesTurnipV26R8AsSwitchIncompatible() {
        assertTrue(metadata("Mesa Turnip driver", "v26.0.0 - R8-v1").isKnownSwitchIncompatibleRelease)
    }

    @Test
    fun leavesSystemAndOtherTurnipReleasesAvailable() {
        assertFalse(metadata("System Driver", "").isKnownSwitchIncompatibleRelease)
        assertFalse(metadata("Mesa Turnip driver", "v25.3.0 R14").isKnownSwitchIncompatibleRelease)
        assertFalse(metadata("Qualcomm driver", "v26.0.0 R8").isKnownSwitchIncompatibleRelease)
    }
}
