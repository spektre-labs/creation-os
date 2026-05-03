{{/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */}}
{{- define "creation-os.name" -}}
{{- default .Chart.Name .Values.nameOverride | trunc 63 | trimSuffix "-" }}
{{- end }}

{{- define "creation-os.fullname" -}}
{{- if .Values.fullnameOverride }}
{{- .Values.fullnameOverride | trunc 63 | trimSuffix "-" }}
{{- else }}
{{- $name := default .Chart.Name .Values.nameOverride }}
{{- if contains $name .Release.Name }}
{{- .Release.Name | trunc 63 | trimSuffix "-" }}
{{- else }}
{{- printf "%s-%s" .Release.Name $name | trunc 63 | trimSuffix "-" }}
{{- end }}
{{- end }}
{{- end }}

{{- define "creation-os.labels" -}}
helm.sh/chart: {{ include "creation-os.chart" . }}
{{ include "creation-os.selectorLabels" . }}
app.kubernetes.io/managed-by: {{ .Release.Service }}
{{- end }}

{{- define "creation-os.selectorLabels" -}}
app.kubernetes.io/name: {{ include "creation-os.name" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}

{{- define "creation-os.chart" -}}
{{- printf "%s-%s" .Chart.Name .Chart.Version | replace "+" "_" }}
{{- end }}
