import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Creation OS — measure first",
  description:
    "σ-gate lab surface: documentation and playground. Prose follows docs/CLAIM_DISCIPLINE.md for headline metrics.",
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body className="antialiased">{children}</body>
    </html>
  );
}
